#include <cstdint>

struct SuperBlock {
    uint16_t s_inodes_count;
    uint16_t s_blocks_count;
    uint16_t s_free_inodes_count;
    uint16_t s_free_blocks_count;
    uint16_t s_first_data_block;
    uint16_t s_log_block_size;
};

struct Inode {
    uint16_t i_mode;
    uint16_t i_uid;
    uint16_t i_gid;
    uint32_t i_size;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_links_count;
    uint16_t i_blocks;
    uint16_t i_block[6];
};

struct DirectoryEntry {
    uint16_t inode;
    uint8_t name_len;
    uint8_t file_type;
    char name[32];
};

struct Address {
    uint16_t addr;
};