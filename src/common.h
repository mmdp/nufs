#ifndef NUFS_COMMON_H
#define NUFS_COMMON_H

#include <string>

const int g_block_size=512;
const int g_inode_size=64;
const int g_dir_entry_size=32;

void log(int, std::string);
void log(std::string, int);
void log(std::string, std::string);

int power2(int x);


#endif //NUFS_COMMON_H
