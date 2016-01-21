#ifndef NUFS_FSPROGS_H
#define NUFS_FSPROGS_H

#include "structs.h"
#include "common.h"
#include <fstream>
#include <sys/stat.h>
#include <string.h>


class fsprogs {
private:
    std::fstream device_f;
    SuperBlock sb;
    char empty_block[g_block_size];

public:
    fsprogs(std::string device_file_name);
    ~fsprogs();

    // Create file system
    bool create_fs(uint16_t blocks_count);

    // Create new Inode
    uint16_t create_inode(uint16_t inode_type, uint16_t uid, uint16_t gid);
    // Update Inode
    int update_inode(uint16_t i_pos, Inode* inode);
    // Get a Inode
    Inode* read_inode(uint16_t i_pos);

    // Create empty file (, directory ...)
    bool create_file(uint8_t file_type, std::string file_name, std::string path, uint16_t uid, uint16_t gid);

    // Create directory
    bool create_directory(std::string dir_name, std::string path, uint16_t uid, uint16_t gid);

    // Create empty regular file
    bool touch_file(std::string file_name, std::string path, uint16_t uid, uint16_t gid);

    // Write data to file
    bool write_to_file(std::string file_path, std::string data);

    // Read data from file
    std::string read_file(std::string file_path);

    // Remove file
    bool remove_file(std::string file_path, bool force=false, bool recursive=false);

    // Remove empty directory
    bool remove_dir(std::string dir_path);

    bool add_link(std::string file_path, std::string dest_path);

    // Get items in directory
    std::string* get_items(std::string path, bool);

    // Get last Inode of "path"
    Inode* get_last_inode(std::string path);
    int get_last_inode_pos(std::string path);

private:

    // Check directory path
    std::string check_path(std::string path);

    bool remove_file_bits(uint16_t i_pos, Inode* inode_p);

    // Get and set bits from inode or data bitmap
    int* get_empty_bit(char *bitmap, int size);
    // Unset taken bits
    bool recover_bitmap(char* bitmap, int* bits, int size);

    std::fstream get_fsteam(std::string device_file_name);

    bool write_to_device(const char *data, int b_pos, unsigned long size=g_block_size);
};


#endif //NUFS_FSPROGS_H
