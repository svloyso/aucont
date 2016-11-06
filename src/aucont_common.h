//
// Created by svloyso on 06.11.16.
//

#ifndef AUCONT_AUCONT_COMMON_H
#define AUCONT_AUCONT_COMMON_H

#include <vector>

void reg_cont(int pid);
void unreg_cont(int pid);
void list_cont(std::vector<int>& v);

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)
#endif //AUCONT_AUCONT_COMMON_H
