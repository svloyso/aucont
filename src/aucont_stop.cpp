//
// Created by svloyso on 06.11.16.
//

#include <iostream>

#include <signal.h>
#include <stdlib.h>

static void usage(const char* pname) {
	std::cout << "usage: " << pname << " PID [SIG_NUM]" << std::endl;
	std::cout << "Kills root container process with signal and cleans up container resources" << std::endl;
	std::cout << "PID - container init process pid in its parent PID namespace" << std::endl;
	std::cout << "SIG_NUM - number of signal to send to container process" << std::endl;
	std::cout << "          default is SIGTERM (15)" << std::endl;
	exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {

	if(argc < 2 || argc > 3)
		usage(argv[0]);

	int pid = atoi(argv[1]);
	int sig = SIGTERM;
	if(argc > 2)
		sig = atoi(argv[2]);

	kill(pid, sig);
}
