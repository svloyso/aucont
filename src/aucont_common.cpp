#include <fstream>
#include <vector>
#include <iterator>
#include <algorithm>

#include "aucont_common.h"

static const char* LISTFILE = ".aucont";

void reg_cont(int pid) {
	std::ofstream f;
	f.open(LISTFILE, std::ios_base::out | std::ios_base::app);
	f << pid << std::endl;
	f.close();
}

void unreg_cont(int pid) {
	std::ifstream fi(LISTFILE);

	std::vector<int> pids;
	list_cont(pids);
	pids.erase(std::remove(pids.begin(), pids.end(), pid), pids.end());
	fi.close();

	std::ofstream fo(LISTFILE);
	std::copy(pids.begin(), pids.end(), std::ostream_iterator<int>(fo, "\n"));
	fo.close();
}


void list_cont(std::vector<int>& l) {
	std::ifstream fi(LISTFILE);
	std::copy(std::istream_iterator<int>(fi), std::istream_iterator<int>(), std::back_inserter(l));
	fi.close();
}
