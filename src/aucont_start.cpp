#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include <sched.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <wait.h>

#include <linux/limits.h>
#include <sys/stat.h>
#include <syscall.h>
#include <sys/mount.h>

#include "aucont_common.h"

const int STACK_SIZE = 1024 * 1024;
char stack[STACK_SIZE];

static void usage(const char* pname) {
	std::cout << "usage: " << pname << " [-d --cpu CPU_PERC --net IP] IMAGE_PATH CMD [ARGS]" << std::endl;
	std::cout << "IMAGE_PATH - path to image of container file system" << std::endl;
	std::cout << "CMD - command to run inside container" << std::endl;
	std::cout << "ARGS - arguments for CMD" << std::endl;
	std::cout << "-d - daemonize" << std::endl;
	std::cout << "--cpu CPU_PERC - percent of cpu resources allocated for container 0..100" << std::endl;
	std::cout << "--net IP - create virtual network between host and container." << std::endl;
	std::cout << "      IP - container ip address, IP+1 - host side ip address." << std::endl;
	exit(EXIT_FAILURE);
}

struct context {
	int uid;
	int gid;
	char** argv;
	char* root;
	bool daemon;
    int pipe[2];
	bool set_net;
	char net[16];
	char host_net[16];
};

void setup_mount(const char* root) {

	if (mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL))
		errExit("setup_mount:mount");

	char path[100];
	sprintf(path, "%s/proc", root);
	if (mount("proc", path, "proc", MS_NOEXEC, NULL))
		errExit("setup_mount:mount");

	sprintf(path, "%s/sys", root);
	if (mount("sysfs", path, "sysfs", MS_NOEXEC, NULL))
		errExit("setup_mount:mount");

	char const * old_root = ".old_root_basta228";
	sprintf(path, "%s/%s", root, old_root);
	if (access(path, F_OK) && mkdir(path, 0777))
		errExit("setup_mount:mkdir");

	if (mount(root, root, "bind", MS_BIND | MS_REC, NULL)) {
		errExit("setup_mount:bind");
	}

	sprintf(path, "%s/%s", root, old_root);
	if (syscall(SYS_pivot_root, root, path)) {
		errExit("setup_mount:syscall");
	}

	if (chdir("/")) {
		errExit("setup_mount:chdir");
	}

	sprintf(path, "/%s", old_root);
	if (umount2(path, MNT_DETACH))
		errExit("setup_mount:umount2");

}

void set_userns(int pid, int uid, int gid) {
	int fd;
	char filepath[PATH_MAX];
	char line[255]; sprintf(filepath, "/proc/%d/uid_map", pid);
	sprintf(line, "0 %d 1", uid);

	fd = open(filepath, O_RDWR);
	if(fd == -1)
		errExit("open");
	if(write(fd, line, strlen(line)) == -1)
		errExit("write");

	sprintf(filepath, "/proc/%d/setgroups", pid);
	sprintf(line, "deny");

	fd = open(filepath, O_RDWR);
	if(fd == -1)
		errExit("open");
	if(write(fd, line, strlen(line)) == -1)
		errExit("write");

	sprintf(filepath, "/proc/%d/gid_map", pid);
	sprintf(line, "0 %d 1", gid);

	fd = open(filepath, O_RDWR);
	if(fd == -1)
		errExit("open");
	if(write(fd, line, strlen(line)) == -1)
		errExit("write");
}

void set_hostname(const char* hostname) {
	sethostname(hostname, strlen(hostname));
}

#define CPU_PATH "/sys/fs/cgroup/cpu"

void set_cpu(int perc, int pid) {
	char path[100];
	char group_path[255];
	char cmd[200];
	size_t cpu_num;

	if(access(CPU_PATH, F_OK)) {
		if(system("sudo mkdir -p " CPU_PATH))
			errExit("mkdir " CPU_PATH);
		if(system("sudo mount -t cgroup -ocpu " CPU_PATH))
			errExit("mount cgroup");
	}

	sprintf(group_path, CPU_PATH "/group%d", pid);
	sprintf(cmd, "sudo mkdir -p %s", group_path);
	std::cerr << cmd << std::endl;
	if (system(cmd))
		errExit("mkdir:cgroup");

	cpu_num = sysconf(_SC_NPROCESSORS_ONLN);

	strcpy(path, group_path);
	strcat(path, "/cpu.cfs_period_us");
	sprintf(cmd, "echo %d | sudo tee --append %s ", 1000000, path);
	std::cerr << cmd << std::endl;
	if (system(cmd) < 0)
		errExit("cgroup:set_period");

	strcpy(path, group_path);
	strcat(path, "/cpu.cfs_quota_us");
	sprintf(cmd, "echo %ld | sudo tee --append %s > /dev/null", (1000000 * cpu_num * perc) / 100, path);
	std::cerr << cmd << std::endl;
	if (system(cmd) < 0)
		errExit("cgroup:set_quota");

	strcpy(path, group_path);
	sprintf(path, "/tasks");
	sprintf(cmd, "echo %d | sudo tee --append %s > /dev/null", pid, path);
	std::cerr << cmd << std::endl;
	if (system(cmd))
		errExit("cgroup:add_tasks");
}

void setup_net_cont(const char* ip_host, const char* ip_cnt, int pid) {
	char cmd[256];

	sprintf(cmd, "ip link set lo up");
	if (system(cmd))
		errExit("set lo");

	sprintf(cmd, "ip addr add %s/24 dev vethc%d", ip_cnt, pid);
	if (system(cmd))
		errExit("add veth addr cont");

	sprintf(cmd, "ip link set vethc%d up", pid);
	if (system(cmd))
		errExit("set veth up cont");

	sprintf(cmd, "ip route add default via %s", ip_host);
	if (system(cmd))
		errExit("set gateway");
}

void setup_net_host(const char* ip_host, int pid) {
	char cmd[255];

	sprintf(cmd, "sudo ip link add vethh%d type veth peer name vethc%d", pid, pid);
	if (system(cmd))
		errExit("create veth");

	sprintf(cmd, "sudo ip link set vethc%d netns %d", pid, pid);
	if (system(cmd))
		errExit("set veth");

	sprintf(cmd, "sudo ip addr add %s/24 dev vethh%d", ip_host, pid);
	if (system(cmd))
		errExit("add veth addr host");

	sprintf(cmd, "sudo ip link set vethh%d up", pid);
	if (system(cmd))
		errExit("set veth up host");
}


int cont_func(void* arg) {
	context* cnt = (context*)arg;

	set_hostname("container");

    int pid;
    read(cnt->pipe[0], &pid, sizeof(pid));
    close(cnt->pipe[0]);
    close(cnt->pipe[1]);

	set_userns(pid, cnt->uid, cnt->gid);
	setup_mount(cnt->root);

	if(cnt->set_net)
		setup_net_cont(cnt->host_net, cnt->net, pid);
    
    if(cnt->daemon) {
	    umask(0);
		setsid();
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
		open("/dev/null", O_RDONLY);
		open("/dev/null", O_RDWR);
		open("/dev/null", O_RDWR);
    }

	execvp(*cnt->argv, cnt->argv);
    return 0; // never reached
}

int main(int argc, char* argv[]) {
	int opt;
	static struct option long_options[] = {
			{"cpu", required_argument, 0, 'c'},
			{"net", required_argument, 0, 'n'},
			{0, 0, 0, 0}
	};

	bool daemonize = false;
	int cpu = 100;
	bool set_net = false;
	char net[16];

	while ((opt = getopt_long(argc, argv, "d", long_options, NULL)) != -1) {
		switch (opt) {
			case 'd':
				daemonize = true;
				break;
			case 'c':
				cpu = atoi(optarg);
                if(cpu < 0 || cpu > 100) {
                    std::cout << "err: cpu param is out of range" << std::endl;
                    return EXIT_FAILURE;
                }
				break;
			case 'n':
				set_net = true;
				if(strlen(optarg) > 15) {
					std::cout << "Invalid ip addr" << std::endl;
					return 1;
				}
				strcpy(net, optarg);
				break;
			default:
				usage(argv[0]);
		}
	}

	if(optind >= argc)
		usage(argv[0]);

	int flags = CLONE_NEWIPC | CLONE_NEWNET | CLONE_NEWNS | CLONE_NEWUSER | CLONE_NEWUTS | CLONE_NEWPID | SIGCHLD;

	context cnt;
	cnt.root = argv[optind];
	cnt.argv = &argv[optind + 1];
	cnt.uid = getuid();
	cnt.gid = getgid();
	cnt.daemon = daemonize;
	cnt.set_net = set_net;
	if(set_net) {
		strcpy(cnt.net, net);
		strcpy(cnt.host_net, net);
		char* p = strrchr(cnt.host_net, '.');
		int t = atoi(p + 1);
		sprintf(p + 1, "%d", t + 1);
	}

    if(pipe(cnt.pipe) == -1)
        errExit("pipe");

    int child_pid;
	if((child_pid = clone(cont_func, stack + STACK_SIZE, flags, &cnt)) == -1)
	    errExit("clone");

	std::cout << child_pid << std::endl;
    reg_cont(child_pid);

    if(cpu != 100) {
        set_cpu(cpu, child_pid);
    }

	if(set_net) {
		setup_net_host(cnt.host_net, child_pid);
	}

    write(cnt.pipe[1], &child_pid, sizeof(child_pid));

    close(cnt.pipe[0]);
    close(cnt.pipe[1]);

    if(daemonize) {
	    umask(0);
		setsid();
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
		open("/dev/null", O_RDONLY);
		open("/dev/null", O_RDWR);
		open("/dev/null", O_RDWR);
	} else {
        wait(NULL);
    }
	return EXIT_SUCCESS;
}
