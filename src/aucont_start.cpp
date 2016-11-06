//
// Created by svloyso on 06.11.16.
//
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include <sched.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <linux/limits.h>
#include <fcntl.h>
#include <wait.h>
#include <sys/stat.h>
#include <syscall.h>
#include <sys/mount.h>

#include "aucont_common.h"


#define STACK_SIZE (1024 * 1024)
static char child_stack[STACK_SIZE];

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
	int pid;
	int uid;
	int gid;
	char** argv;
	char* root;
	bool daemon;
};

void setup_mount(const char* root) {
	mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL);

	char path[100];
	sprintf(path, "%s/proc", root);
	mount("proc", path, "proc", MS_NOEXEC, NULL);

	sprintf(path, "%s/sys", root);
	mount("sysfs", path, "sysfs", MS_NOEXEC, NULL);

	const char* old_root = ".old_root";
	sprintf(path, "%s/%s", root, old_root);
	mkdir(path, 0777);

	mount(root, root, "bind", MS_BIND | MS_REC, NULL);
	sprintf(path, "%s/%s", root, old_root);
	syscall(SYS_pivot_root, root, path);

	chdir("/");

	sprintf(path, "/%s", old_root);
	umount2(path, MNT_DETACH);
}

void set_uts(int uid, int gid) {
	int fd;
	char filepath[PATH_MAX];
	char line[255];
	sprintf(filepath, "/proc/%d/uid_map", getpid());
	sprintf(line, "0 %d 1", uid);

	fd = open(filepath, O_RDWR);
	if(fd == -1)
		errExit("open");
	if(write(fd, line, strlen(line)) == -1)
		errExit("write");

	sprintf(filepath, "/proc/%d/setgroups", getpid());
	sprintf(line, "deny");

	fd = open(filepath, O_RDWR);
	if(fd == -1)
		errExit("open");
	if(write(fd, line, strlen(line)) == -1)
		errExit("write");

	sprintf(filepath, "/proc/%d/gid_map", getpid());
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
	static const char* contname = "container";
	context* cnt = (context*)arg;
	reg_cont(getpid());
	std::cout << getpid() << std::endl;

	if(cnt->daemon) {
		umask(0);
		setsid();
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
	}

	set_hostname(contname);

	set_uts(cnt->uid, cnt->gid);
	setup_mount(cnt->root);
	execvp(*cnt->argv, cnt->argv);
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

	int flags = CLONE_NEWIPC | CLONE_NEWNET | CLONE_NEWNS | CLONE_NEWUSER | CLONE_NEWUTS;

	context cnt;
	cnt.root = argv[optind];
	cnt.argv = &argv[optind + 1];
	cnt.uid = getuid();
	cnt.gid = getgid();
	cnt.daemon = daemonize;


	if(daemonize) {
		clone(cont_func, child_stack + STACK_SIZE, flags, &cnt);
	} else {
		if(unshare(flags) == -1)
			errExit("unshare");
		cont_func(&cnt);
	}
	return EXIT_SUCCESS;
}
