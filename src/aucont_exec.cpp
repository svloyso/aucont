
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sstream>
#include <stdlib.h>
#include <stdio.h>

#include "aucont_common.h"

void usage(char* pname) {
	std::cout << "usage: " << pname << " PID CMD [ARGS]" << std::endl;
	std::cout << "PID - container init process pid in its parent PID namespace" << std::endl;
	std::cout << "CMD - command to run inside container" << std::endl;
	std::cout << "ARGS - arguments for CMD	" << std::endl;
	exit(EXIT_FAILURE);
}

void enter_ns(int pid, const char* nsname) {
	std::stringstream ss;
	ss << "/proc/" << pid << "/ns/" << nsname;
	int ns_fd = open(ss.str().c_str(), O_RDONLY);
	if(ns_fd == -1)
		errExit("open");
	if(setns(ns_fd, 0))
		errExit("setns");
	close(ns_fd);
}

int main(int argc, char* argv[]) {
	if(argc < 3)
		usage(argv[0]);

	int pid = atoi(argv[1]);
	enter_ns(pid, "user");
	enter_ns(pid, "ipc");
	enter_ns(pid, "net");
	enter_ns(pid, "pid");
	enter_ns(pid, "uts");
	enter_ns(pid, "mnt");

	execvp(argv[2], &argv[2]);
}
