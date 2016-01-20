#ifndef NUFS_MKFS_H
#define NUFS_MKFS_H

#include <stdio.h>
#include <stdint.h>
#include <string>
#include "fsprogs.h"

// create device
bool create_device_file(std::string device_file_name, uint16_t blocks_count);
bool create_base_tree(fsprogs*);

#endif //NUFS_MKFS_H
