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

void set_uts(int pid, int uid, int gid) {
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


int cont_func(void* arg) {
	context* cnt = (context*)arg;

	set_hostname("container");

    int pid;
    read(cnt->pipe[0], &pid, sizeof(pid));
    close(cnt->pipe[0]);
    close(cnt->pipe[1]);

	set_uts(pid, cnt->uid, cnt->gid);
	setup_mount(cnt->root);
    
    if(cnt->daemon) {
	    umask(0);
		setsid();
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
    }

	execvp(*cnt->argv, cnt->argv);
    return 0; // never reached
}

int main(int argc, char* argv[]) {
	int opt;
	static struct option long_options[] = {
			{"cpu", optional_argument, 0, 'c'},
			{"net", optional_argument, 0, 'n'},
			{0, 0, 0, 0}
	};

	bool daemonize = false;
	int cpu;
	char net[16];

	while ((opt = getopt_long(argc, argv, "d", long_options, NULL)) != -1) {
		switch (opt) {
			case 'd':
				daemonize = true;
				break;
			case 'c':
				cpu = atoi(optarg);
				break;
			case 'n':
				strncpy(net, optarg, 16);
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

    if(pipe(cnt.pipe) == -1) 
        errExit("pipe");

    int child_pid;
	if((child_pid = clone(cont_func, stack + STACK_SIZE, flags, &cnt)) == -1)
	    errExit("clone");

	std::cout << child_pid << std::endl;
    reg_cont(child_pid);

    write(cnt.pipe[1], &child_pid, sizeof(child_pid));

    close(cnt.pipe[0]);
    close(cnt.pipe[1]);

    if(daemonize) {
	    umask(0);
		setsid();
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
	} else {
        wait(NULL);
    }
	return EXIT_SUCCESS;
}
