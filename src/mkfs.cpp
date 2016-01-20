#include <fstream>
#include <string.h>
#include <iostream>
#include "common.h"
#include "mkfs.h"
#include "fsprogs.h"

/**
 * Create device file
 *
 */
bool create_device_file(std::string device_file_name, uint16_t blocks_count) {
    // check count <= 4096
    if (blocks_count > 4096 || blocks_count == 0) {
        log(1, "Invalid size");
        return false;
    }

    // all blocks = count + (count + 7) / 8 + 3
    int blocks = blocks_count + (blocks_count + 7) / 8 + 3;

    // check path
    std::ifstream device_if(device_file_name.c_str());
    if (device_if.good()) {
        device_if.close();
        log(1, "Device file exists.");
        return false;
    } else {
        device_if.close();
    }

    // create device file
    char block[g_block_size];
    memset(block, '\0', g_block_size);
    std::ofstream device_f(device_file_name.c_str(), std::ios::out | std::ios::binary);
    for (int i = 0; i < blocks; i++) {
        device_f.write(block, g_block_size);
    }
    device_f.close();

    // return FILE*
    return true;
}

/*
 * Create base system tree
 */
bool create_base_tree(fsprogs *fsp) {
    fsp->create_directory("boot", "/", 0, 0);
    fsp->create_directory("etc", "/", 0, 0);
    fsp->create_directory("bin", "/", 0, 0);
    fsp->create_directory("lib", "/", 0, 0);
    fsp->create_directory("usr", "/", 0, 0);
    fsp->create_directory("var", "/", 0, 0);
    fsp->create_directory("sys", "/", 0, 0);
    fsp->create_directory("run", "/", 0, 0);
    fsp->create_directory("dev", "/", 0, 0);
    fsp->create_directory("proc", "/", 0, 0);
    fsp->create_directory("root", "/", 0, 0);
    fsp->create_directory("mnt", "/", 0, 0);
    fsp->create_directory("home", "/", 0, 0);
    return true;
}
