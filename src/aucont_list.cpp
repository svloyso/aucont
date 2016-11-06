//
// Created by svloyso on 06.11.16.
//
#include <iostream>
#include <vector>
#include <algorithm>
#include <iterator>

#include <unistd.h>
#include <sstream>

#include "aucont_common.h"

int main() {
	sleep(1);
	std::vector<int> conts;
	list_cont(conts);

	for(int i = 0; i < conts.size(); ++i) {
		std::stringstream ss;
		ss << "/proc/" << conts[i];
		if(access(ss.str().c_str(), F_OK) == -1) {
			unreg_cont(conts[i]);
		} else {
			std::cout << conts[i] << std::endl;
		}
	}
}
