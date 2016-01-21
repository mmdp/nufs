#include <iostream>
#include <libgen.h>
#include "fsprogs.h"

fsprogs::fsprogs(std::string device_file_name) {
    this->device_f = get_fsteam(device_file_name);

    // Read Super Block
    this->sb = SuperBlock();
    device_f.seekp(0, std::ios::beg);
    device_f.read((char*)&sb, sizeof(SuperBlock));

    memset(empty_block, '\0', g_block_size);
}

std::fstream fsprogs::get_fsteam(std::string device_file_name) {
    std::fstream device_f(device_file_name.c_str(), std::ios::in | std::ios::out | std::ios::binary);
    return device_f;
}

fsprogs::~fsprogs() {
    this->device_f.close();
}

bool fsprogs::create_fs(uint16_t blocks_count) {

    // Super block
    sb.s_inodes_count = blocks_count;
    sb.s_blocks_count = blocks_count;
    sb.s_free_inodes_count = uint16_t (blocks_count - 1);
    sb.s_free_blocks_count = uint16_t (blocks_count - 1);
    sb.s_first_data_block = uint16_t ((blocks_count + 7) / 8 + 3);
    sb.s_log_block_size = g_block_size;
    device_f.seekp(0, std::ios::beg);
    device_f.write((char*)&sb, sizeof(SuperBlock));

    // Create root directory

    // Inode table
    Inode ri = Inode();
    ri.i_mode = S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    ri.i_uid = 0;
    ri.i_gid = 0;
    ri.i_size = 0;  // TODO dir size = sub dir count, file size = file size (Byte)
    timespec time1;
    if (clock_gettime(CLOCK_REALTIME, &time1)) {
        device_f.flush();
        return false;
    }
    ri.i_ctime = (uint32_t) time1.tv_sec;
    ri.i_atime = ri.i_ctime;
    ri.i_mtime = ri.i_ctime;
    ri.i_dtime = 0;
    ri.i_links_count = 0;
    ri.i_blocks = 1;    // dir
    ri.i_block[0] = sb.s_first_data_block;
    device_f.seekp(g_block_size * 3, std::ios::beg);
    device_f.write((char*)&ri, sizeof(ri));


    // Data (empty)
    //device_f.seekp(g_block_size * sb.s_first_data_block, std::ios::beg);

    // Bitmaps
    char byte;
    device_f.seekp(g_block_size, std::ios::beg);
    device_f.read(&byte, 1);
    byte |= 1;
    device_f.seekp(g_block_size, std::ios::beg);
    device_f.write(&byte, 1);

    device_f.seekp(g_block_size * 2, std::ios::beg);
    device_f.read(&byte, 1);
    byte |= 1;
    device_f.seekp(g_block_size * 2, std::ios::beg);
    device_f.write(&byte, 1);

    device_f.flush();

    return true;
}


int fsprogs::update_inode(uint16_t i_pos, Inode* inode) {
    device_f.seekp(g_block_size * 3 + i_pos * g_inode_size, std::ios::beg);
    device_f.write((char*)inode, g_inode_size);
    device_f.flush();
    // Unknown: flush not work ?
    //log("orig node size", inode->i_size);
    //log("tmp node ctime", read_inode(i_pos)->i_ctime);
    return 0;
}

uint16_t fsprogs::create_inode(uint16_t inode_type, uint16_t uid, uint16_t gid) {
    // get empty from inode bitmap
    char i_bitmap[g_block_size];
    device_f.seekp(g_block_size * 2, std::ios::beg);
    device_f.read(i_bitmap, g_block_size);
    int* i_pos_p = get_empty_bit(i_bitmap, 1);
    if (i_pos_p[0] < 0) {
        // TODO failed
    }
    //std::cout << "i_pos_tmp: " << i_pos_tmp << std::endl;
    uint16_t i_pos = (uint16_t) i_pos_p[0];
    delete i_pos_p;

    // new Inode()
    Inode ni = Inode();
    // if dir
    switch (inode_type) {
        case S_IFDIR:
            ni.i_mode = S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
            break;
        case S_IFREG:
            ni.i_mode = S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
            break;
        default:
            break;
    }
    ni.i_uid = uid;
    ni.i_gid = gid;
    ni.i_size = 0;  // TODO
    timespec time1;
    if (clock_gettime(CLOCK_REALTIME, &time1)) {
        // TODO error
    }
    ni.i_ctime = (uint32_t) time1.tv_sec;
    ni.i_atime = ni.i_ctime;
    ni.i_mtime = ni.i_ctime;
    ni.i_dtime = 0;
    ni.i_links_count = 1;
    ni.i_blocks = 1;    // TODO

    // TODO delay
    // Set Data Block
    char d_bitmap[g_block_size];
    device_f.seekp(g_block_size, std::ios::beg);
    device_f.read(d_bitmap, g_block_size);
    int* d_pos_p = get_empty_bit(d_bitmap, 1);
    //log(0, "d pos p: " + std::to_string(d_pos_p[0]));
    if (d_pos_p[0] < 0) {
        // TODO failed
    }
    ni.i_block[0] = (uint16_t) (d_pos_p[0] + sb.s_first_data_block);
    delete d_pos_p;

    // write to inode table
    device_f.seekp(g_block_size * 3 + i_pos * g_inode_size, std::ios::beg);
    device_f.write((char*)&ni, sizeof(ni));

    // write to inode bitmap
    device_f.seekp(g_block_size * 2, std::ios::beg);
    device_f.write(i_bitmap, g_block_size);

    // write to data bitmap
    device_f.seekp(g_block_size, std::ios::beg);
    device_f.write(d_bitmap, g_block_size);

    device_f.flush();

    return i_pos;
}

bool fsprogs::write_to_file(std::string file_path, std::string data) {

    unsigned long last_d = file_path.find_last_of('/');
    if (last_d < 0 || last_d + 1 == file_path.length()) {
        return false;
    }

    // Check data size
    if (data.size() > sb.s_free_blocks_count * g_block_size)
        return false;   // no enough space
    if (data.size() == 0) {
        // TODO do empty file
        return true;    // nothing to write
    }

    //std::string path = file_path.substr(0, last_d);
    //std::string file_name = file_path.substr(last_d);

    // Get file inode
    int file_i_pos = get_last_inode_pos(file_path);
    //log(0, "file path: " + file_path);
    //log(0, "file i pos: " + std::to_string(file_i_pos));
    if (file_i_pos < 0) {
        return false;
    }
    Inode* inode_p = read_inode((uint16_t) file_i_pos);
    //log(0, "file i pos: " + std::to_string(file_i_pos));

    // TODO delay allocation
    int data_blocks = (int) ((data.size() + 511) / g_block_size);
    uint16_t file_data_pos[data_blocks];
    file_data_pos[0] = inode_p->i_block[0];

    // Get more empty blocks
    char d_bitmap[g_block_size];
    int need_blocks = data_blocks;
    if (data_blocks > 1) {
        if (data_blocks > 4) {
            if (data_blocks > 260) {
                need_blocks = data_blocks + 2 + (data_blocks-5)/256;
            } else {
                need_blocks = data_blocks + 1;
            }
        }
        // Read data bitmap
        device_f.seekp(g_block_size, std::ios::beg);
        device_f.read(d_bitmap, g_block_size);
        log(0, "Read data bits");
        for (int kk =0; kk < 4; kk++) {
            log("k", kk);
            log("bitmap idx1", d_bitmap[kk]);
        }
        int* d_pos_p = get_empty_bit(d_bitmap, need_blocks-1);  // first is allocated before
        log(0, "Read data bits");
        for (int kk =0; kk < 4; kk++) {
            log("k", kk);
            log("bitmap idx1", d_bitmap[kk]);
        }
        if (d_pos_p[0] < 0) {
            // TODO failed
            // Unset taken data bit here?
            recover_bitmap(d_bitmap, d_pos_p, need_blocks-1);
            return false;
        }
        for (int i = 1; i < need_blocks; i++) {
             file_data_pos[i] = (uint16_t) d_pos_p[i-1] + sb.s_first_data_block;
        }
    }

    int blk_idx = 0;
    uint16_t container_block[256] = {};
    uint16_t container1_block[256] = {};
    uint16_t container2_block[256] = {};
    // loop all data parts
    for (int i=0; i < data_blocks; i++) {
        // get data
        std::string tmp_string = data.substr((unsigned long) (i * g_block_size), g_block_size);
        log(0, std::to_string(i));
        log(0, tmp_string);
        const char *tmp_data = tmp_string.c_str();
        if (i<4) {
            inode_p->i_block[i] = file_data_pos[blk_idx++];
            log(0, "i block i");
            log(0, std::to_string(inode_p->i_block[i]));
            // TODO inline fun to write
            int d_pos = inode_p->i_block[i];
            write_to_device(tmp_data, d_pos, tmp_string.size());
            // TODO return int
        } else if (i>=4 && i<260) {
            if (i == 4) {
                inode_p->i_block[4] = file_data_pos[blk_idx++];
                log(0, "i block 4");
                log(0, std::to_string(inode_p->i_block[4]));
            }
            // Write data pos to 4th block container
            container_block[i-4] = file_data_pos[blk_idx++];
            log(0, "container 0 i 4");
            log(0, std::to_string(container_block[i-4]));
            // Write data to device
            int d_pos = container_block[i-4];
            write_to_device(tmp_data, d_pos, tmp_string.size());
            if (i == 259 || i+1 == data_blocks) {
                // Write 4th block to device
                write_to_device((const char *) container_block, inode_p->i_block[4]);
            }
        } else {
            // Allocate 1st container
            if (i == 260) {
                inode_p->i_block[5] = file_data_pos[blk_idx++];
            }

            // 1st container index
            int idx1 = (i-260) / 256;
            // 2nd container index
            int idx2 = (i-4) % 256;

            // Allocate 2nd container (Write 2nd container pos to 1st container)
            if (idx2 == 0) {
                container1_block[idx1] = file_data_pos[blk_idx++];
            }

            // Allocate data block (Write data pos to 2nd container block)
            container2_block[idx2] = file_data_pos[blk_idx++];

            // Write data to device
            int d_pos = container2_block[idx2];
            write_to_device(tmp_data, d_pos, tmp_string.size());

            // Write 2nd container to device
            if (idx2 == 255 || i+1 == data_blocks) {
                d_pos = container1_block[idx1];
                write_to_device((const char *) container2_block, d_pos);
            }

            // Write 1st container to device
            if (i+1 == data_blocks) {
                d_pos = inode_p->i_block[5];
                write_to_device((const char *) container1_block, d_pos);
            }
        }
    }

    inode_p->i_blocks = (uint16_t) data_blocks;
    inode_p->i_size = (uint32_t) data.size();

    // Write back to inode table
    device_f.seekp(g_block_size * 3 + file_i_pos * g_inode_size, std::ios::beg);
    device_f.write((char*) inode_p, sizeof(Inode));
    device_f.flush();

    // Read inode bitmap
    char i_bitmap[g_block_size];
    device_f.seekp(g_block_size * 2, std::ios::beg);
    device_f.read(i_bitmap, g_block_size);

    // Write to data bitmap
    device_f.seekp(g_block_size, std::ios::beg);
    device_f.write(d_bitmap, g_block_size);

    // Write to inode bitmap
    device_f.seekp(g_block_size * 2, std::ios::beg);
    device_f.write(i_bitmap, g_block_size);

    device_f.flush();

    delete inode_p;

    return true;
}

std::string fsprogs::read_file(std::string file_path) {
    // TODO relative path
    unsigned long last_d = file_path.find_last_of('/');
    if (last_d < 0 || last_d + 1 == file_path.length()) {
        // TODO raise error ?
        log(1, "Invalid path.");
        return "";
    }

    int file_i_pos = get_last_inode_pos(file_path);
    if (file_i_pos < 0) {
        // TODO raise error ?
        log(1, "Inode error!");
        return "";
    }
    Inode* inode_p = read_inode((uint16_t) file_i_pos);
    if (inode_p->i_mode & S_IFDIR) {
        // TODO
        log(1, "This is a directory.");
        return "";
    }

    std::string data = "";
    std::string tmp_string = "";
    char block_data[g_block_size];
    uint16_t container1_block[256] = {};
    uint16_t container2_block[256] = {};
    for (int i=0; i < inode_p->i_blocks; i++) {
        if (i < 4) {
            device_f.seekp(g_block_size * inode_p->i_block[i], std::ios::beg);
            device_f.read(block_data, sizeof(char) * g_block_size);
        } else if (i>=4 && i<260) {
            if (i==4) {
                device_f.seekp(g_block_size * inode_p->i_block[4], std::ios::beg);
                device_f.read((char *) container1_block, sizeof(char) * g_block_size);
            }
            int d_pos = container1_block[i-4];
            device_f.seekp(g_block_size * d_pos, std::ios::beg);
            device_f.read(block_data, g_block_size);
        } else {
            if (i == 260) {
                // Read 1st container
                device_f.seekp(g_block_size * inode_p->i_block[5], std::ios::beg);
                device_f.read((char *) container1_block, sizeof(char) * g_block_size);
            }
            int idx1 = (i-260) / 256;
            int idx2 = (i-260) % 256;
            if (idx2 == 0) {
                // Read 2nd container
                device_f.seekp(g_block_size * container1_block[idx1], std::ios::beg);
                device_f.read((char *) container2_block, sizeof(char) * g_block_size);
            }
            // Read block data
            device_f.seekp(g_block_size * container2_block[idx2], std::ios::beg);
            device_f.read(block_data, g_block_size);
        }
        // FIXME block_data may larger than g_block_size (why?)
        // Workaround
        tmp_string = std::string(block_data);
        unsigned long length = tmp_string.size() > g_block_size ? g_block_size : tmp_string.size();
        //log("tmp str len", (int) length);
        data += tmp_string.substr(0, length);
    }

    return data;
}


bool fsprogs::create_file(uint8_t file_type, std::string file_name, std::string path,
                 uint16_t uid, uint16_t gid) {
    path = check_path(path);
    if (path == "") return false;

    // Get parent dir data position
    uint16_t updir_inode_pos = (uint16_t) get_last_inode_pos(path);
    if (updir_inode_pos < 0) return false;
    Inode *inode_p = read_inode(updir_inode_pos);
    if (!inode_p) return false;
    uint16_t updir_data_pos = inode_p->i_block[0];

    // Write to parent dir content

    DirectoryEntry dir_entry;
    bool success = false; // suppose up dir is full

    // Then try to find an empty entry space
    for (int i = 0; i < 16; i++) {
        device_f.seekp(g_block_size * updir_data_pos + i * g_dir_entry_size, std::ios::beg);
        device_f.read((char *)&dir_entry, sizeof(char) * 32);
        if (dir_entry.inode) {
            continue;
        } else {
            // Create inode and set data block
            uint16_t inode_pos = 0;
            switch (file_type) {
                case 1:
                    inode_pos = create_inode(S_IFREG, uid, gid);
                    break;
                case 2:
                    inode_pos = create_inode(S_IFDIR, uid, gid);
                    break;
                    // TODO other type
                default:
                    break;
            }
            if (inode_pos < 0) break;   // Failed to create inode
            dir_entry.inode = inode_pos;
            dir_entry.name_len = (uint8_t) file_name.size();
            dir_entry.file_type = file_type;
            strcpy(dir_entry.name, file_name.c_str());
            //std::cout << "updir pos: " << updir_data_pos << std::endl;
            //std::cout << "de pos: " << g_block_size * updir_data_pos + i * g_dir_entry_size << std::endl;
            //std::cout << "dir name: " << dir_entry.name << std::endl;
            device_f.seekp(g_block_size * updir_data_pos + i * g_dir_entry_size, std::ios::beg);
            device_f.write((char *)&dir_entry, sizeof(char) * g_dir_entry_size);
            device_f.flush();
            inode_p->i_size += 1;
            //log("inode size", inode_p->i_size);
            update_inode(updir_inode_pos, inode_p);
            success = true;
            break;
        }
    }

    delete inode_p;
    device_f.flush();

    return success;
}

bool fsprogs::create_directory(std::string dir_name, std::string path,
                      uint16_t uid, uint16_t gid) {
    return create_file(2, dir_name, path, uid, gid);
}

bool fsprogs::touch_file(std::string file_name, std::string path,
                uint16_t uid, uint16_t gid) {
    return create_file(1, file_name, path, uid, gid);
}

bool fsprogs::remove_file(std::string file_path, bool force, bool recursive) {

    unsigned long last_d = file_path.find_last_of('/');
    if (last_d < 0 || last_d + 1 == file_path.length()) {
        return false;
    }

    std::string dir_path = file_path.substr(0, last_d);
    std::string file_name = file_path.substr(last_d+1);

    int dir_i_pos = get_last_inode_pos(dir_path);
    Inode* dir_inode_p = get_last_inode(dir_path);
    if(!dir_inode_p) return false;
    uint16_t updir_data_pos = dir_inode_p->i_block[0];

    DirectoryEntry dir_entry;
    for (int i = 0; i < 16; i++) {
        device_f.seekp(g_block_size * updir_data_pos + i * g_dir_entry_size, std::ios::beg);
        device_f.read((char *)&dir_entry, 32);
        if (dir_entry.name == file_name) {
            Inode* inode_p = read_inode(dir_entry.inode);
            if (S_ISDIR(inode_p->i_mode) && inode_p->i_size > 0) {
                return false;
            }
            // Remove from bitmap if only one link
            if (inode_p->i_links_count == 1) {
                remove_file_bits(dir_entry.inode, inode_p);
            }

            // Remove dir entry
            char empty_entry[g_dir_entry_size];
            memset(empty_entry, '\0', g_dir_entry_size);
            device_f.seekp(g_block_size * updir_data_pos + i * g_dir_entry_size, std::ios::beg);
            device_f.write(empty_entry, g_dir_entry_size);
            device_f.flush();
            // Update dir size
            dir_inode_p->i_size -= 1;
            update_inode((uint16_t) dir_i_pos, dir_inode_p);

            delete inode_p;
            delete dir_inode_p;
            return true;
        } else {
            continue;
        }
    }
    delete dir_inode_p;
    return false;
}

bool fsprogs::remove_dir(std::string dir_path) {
    return remove_file(dir_path);
}

bool fsprogs::remove_file_bits(uint16_t i_pos, Inode* inode_p) {
    // Unset inode bitmap
    char i_bitmap[g_block_size];
    device_f.seekp(g_block_size * 2, std::ios::beg);
    device_f.read(i_bitmap, g_block_size);
    int bits[1] = {i_pos};
    recover_bitmap(i_bitmap, bits, 1);
    device_f.seekp(g_block_size * 2, std::ios::beg);
    device_f.write(i_bitmap, g_block_size);

    // Unset data bitmap
    char d_bitmap[g_block_size];
    int d_bits[inode_p->i_blocks];
    uint16_t container1_block[256] = {};
    uint16_t container2_block[256] = {};
    int container_bit[1];
    device_f.seekp(g_block_size, std::ios::beg);
    device_f.read(d_bitmap, g_block_size);
    for (int j = 0; j < inode_p->i_blocks; j++) {
        if (j < 4) {
            d_bits[j] = inode_p->i_block[j] - sb.s_first_data_block;
        } else if (j >= 4 && j < 260) {
            if (j == 4) {
                device_f.seekp(g_block_size * inode_p->i_block[4], std::ios::beg);
                device_f.read((char *) container1_block, g_block_size);
            }
            d_bits[j] = container1_block[j-4] - sb.s_first_data_block;
            if (j == 259 || j+1 == inode_p->i_blocks) {
                container_bit[0] = inode_p->i_block[4] - sb.s_first_data_block;
                recover_bitmap(d_bitmap, container_bit, 1);
            }
        } else {
            if (j == 260) {
                // Read 1st container
                device_f.seekp(g_block_size * inode_p->i_block[5], std::ios::beg);
                device_f.read((char *) container1_block, g_block_size);
            }
            int idx1 = (j-260) / 256;
            int idx2 = (j-260) % 256;
            if (idx2 == 0) {
                // Read 2nd container
                device_f.seekp(g_block_size * container1_block[idx1], std::ios::beg);
                device_f.read((char *) container2_block, g_block_size);
            }
            d_bits[j] = container2_block[idx2] - sb.s_first_data_block;
            // Remove container bits
            if (idx2 == 255 || j+1 == inode_p->i_blocks) {
                container_bit[0] = container1_block[idx1] - sb.s_first_data_block;
                recover_bitmap(d_bitmap, container_bit, 1);
            }
            if (j+1 == inode_p->i_blocks) {
                container_bit[0] = inode_p->i_block[5] - sb.s_first_data_block;
                recover_bitmap(d_bitmap, container_bit, 1);
            }
        }
    }
    recover_bitmap(d_bitmap, d_bits, inode_p->i_blocks);
    device_f.seekp(g_block_size, std::ios::beg);
    device_f.write(d_bitmap, g_block_size);
    device_f.flush();

    return true;
}

bool fsprogs::add_link(std::string file_path, std::string dest_path) {

    int file_i_pos = get_last_inode_pos(file_path);
    if (file_i_pos < 0) return false;
    Inode* file_inode_p = read_inode((uint16_t) file_i_pos);
    //if (!S_ISREG(file_inode_p->i_mode)) {
    //    return false;
    //}

    std::string file_name = basename((char *) file_path.c_str());
    std::string dir_name = dirname(strdup((char *) file_path.c_str()));

    uint16_t dest_i_pos = (uint16_t) get_last_inode_pos(dest_path);
    if (dest_i_pos < 0) return false;
    Inode* dest_inode_p = read_inode((uint16_t) dest_i_pos);
    uint16_t updir_data_pos = dest_inode_p->i_block[0];

    DirectoryEntry dir_entry;
    DirectoryEntry empty_entry;
    int empty_entry_pos = -1;
    for (int i = 0; i < 16; i++) {
        device_f.seekp(g_block_size * updir_data_pos + i * g_dir_entry_size, std::ios::beg);
        device_f.read((char *)&dir_entry, 32);
        if (dir_entry.name == file_name) {
            return false;
        }
        if (!dir_entry.inode) {
            empty_entry = dir_entry;
            empty_entry_pos = i;
        }
    }
    // Found empty space
    if (empty_entry_pos >= 0) {
        empty_entry.inode = (uint16_t) file_i_pos;
        // TODO other file types
        if (S_ISREG(file_inode_p->i_mode)) empty_entry.file_type = 1;
        // TODO disable dir type (except mv)
        if (S_ISDIR(file_inode_p->i_mode)) empty_entry.file_type = 2;
        strcpy(empty_entry.name, file_name.c_str());
        empty_entry.name_len = (uint8_t) file_name.size();
        // Write dest dir data
        device_f.seekp(g_block_size * updir_data_pos + empty_entry_pos * g_dir_entry_size,
                       std::ios::beg);
        device_f.write((char *)&empty_entry, g_dir_entry_size);
        device_f.flush();
        // Update to dest dir inode
        dest_inode_p->i_size += 1;
        update_inode(dest_i_pos, dest_inode_p);
        file_inode_p->i_links_count += 1;
        return true;
    }
    return false;
}


// private
Inode* fsprogs::read_inode(uint16_t i_pos) {
    Inode *inode_p = new Inode();
    device_f.seekp(g_block_size * 3 + i_pos * g_inode_size, std::ios::beg);
    device_f.read((char*)inode_p, sizeof(Inode));
    return inode_p;
}

std::string fsprogs::check_path(std::string path) {
    if (path[path.size()-1] != '/') {
        path += "/";
    }
    if (path[0] != '/') {
        // TODO relative path
        return "";
    }
    return path;
}

int* fsprogs::get_empty_bit(char *bitmap, int size) {
    int * bits = new int[size];
    if (size > 0 && size < sb.s_free_blocks_count) {
        for (int k = 0; k < size;) {
            for (int i = 0; i < g_block_size; i++) {
                //std::cout << "i: " << i << std::endl;
                char c = bitmap[i];
                if (c > -1) {   // c is char: -128 ~ 127
                    //std::cout << "byte i: " << (int) bitmap[i] << std::endl;
                    for (int j=0; j<8; j++) {
                        if (c % 2) {
                            c /= 2;
                        } else {
                            bitmap[i] = (char) (bitmap[i] + power2(j));
                            //std::cout << "i j: " << i << " " << j << std::endl;
                            bits[k] = i * 8 + j;
                            k++;
                            if (k >= size)
                                return bits;
                        }
                    }
                }
            }

        }
    }
    delete bits;
    bits = new int [1];
    bits[0] = -1;
    return bits;
}

bool fsprogs::recover_bitmap(char* bitmap, int* bits, int size) {
    for (int i=0; i<size; i++) {
        int bit_idx1 = bits[i] / 8;
        int bit_idx2 = bits[i] % 8;
        if (bitmap[bit_idx1] & power2(bit_idx2)) {
            // bit is mapped, reset it
            bitmap[bit_idx1] -= power2(bit_idx2);
        } else {
            //log("bit pos", bits[i]);
            //log("bit idx 1", bit_idx1);
            //log("bit idx 2", bit_idx2);
            //for (int kk =0; kk < 4; kk++) {
            //    log("k", kk);
            //    log("bitmap idx1", bitmap[kk]);
            //}
            log(1, "Unset bitmap failed!");
            return false;
        }
    }
    return true;
}

Inode* fsprogs::get_last_inode(std::string path) {
    int pos = get_last_inode_pos(path);
    if (pos >= 0) {
        return read_inode((uint16_t) pos);
    } else {
        return nullptr;
    }
}

int fsprogs::get_last_inode_pos(std::string path) {
    if (path[path.size()-1] != '/') {   // TODO deal in loop below
        path += "/";
    }
    size_t last_d = 1;
    size_t next_d;
    Inode *inode_p = read_inode(0);
    uint16_t updir_data_pos = inode_p->i_block[0];
    DirectoryEntry dir_entry;
    // next dir
    while ((next_d = path.find('/', last_d)) != std::string::npos) {
        std::string dir = path.substr(last_d, next_d - last_d);
        int i = 0;
        // TODO > 16
        for (; i < 16; i++) {
            device_f.seekp(g_block_size * updir_data_pos + i * 32, std::ios::beg);
            device_f.read((char*)&dir_entry, sizeof(char) * 32);
            if (dir_entry.inode) {  // TODO 0 is /
                if (std::string(dir_entry.name) == dir) {
                    delete inode_p;
                    inode_p = read_inode(dir_entry.inode);
                    updir_data_pos = inode_p->i_block[0];
                    break;
                }
            }
        }
        if (i >= 16) {
            // Not found TODO more then one block (16)
            return -1;
        }
        last_d = next_d + 1;
    }
    if (last_d == 1)
        // Root dir inode
        return 0;
    else
        // Other
        return dir_entry.inode;
}

std::string* fsprogs::get_items(std::string path, bool include_details) {
    Inode* dir_inode = get_last_inode(path);
    if (dir_inode == nullptr) {
        return nullptr;
    }

    // TODO size > 16
    std::string *items_p = new std::string[16];

    int updir_data_pos = dir_inode->i_block[0];

    DirectoryEntry dir_entry;
    for (int i = 0; i < 16; i++) {
        device_f.seekp(g_block_size * updir_data_pos + i * g_dir_entry_size, std::ios::beg);
        device_f.read((char *) &dir_entry, sizeof(char) * 32);
        if (include_details) {
            Inode *tmp_inode = read_inode(dir_entry.inode);
            // Skip empty entry TODO 0 ?
            if (!dir_entry.inode) continue;
            // Format time
            time_t rawtime = tmp_inode->i_ctime;
            struct tm * timeinfo;
            timeinfo = localtime (&rawtime);
            std::string tt = asctime(timeinfo);
            tt = tt.substr(0, tt.size()-1);
            // Convert mode
            std::string imode = std::string(S_ISDIR(tmp_inode->i_mode) ? "d" : "-")
                                .append((tmp_inode->i_mode & S_IRUSR) ? "r" : "-")
                                .append((tmp_inode->i_mode & S_IWUSR) ? "w" : "-")
                                .append((tmp_inode->i_mode & S_IXUSR) ? "x" : "-")
                                .append((tmp_inode->i_mode & S_IRGRP) ? "r" : "-")
                                .append((tmp_inode->i_mode & S_IWGRP) ? "w" : "-")
                                .append((tmp_inode->i_mode & S_IXGRP) ? "x" : "-")
                                .append((tmp_inode->i_mode & S_IROTH) ? "r" : "-")
                                .append((tmp_inode->i_mode & S_IWOTH) ? "w" : "-")
                                .append((tmp_inode->i_mode & S_IXOTH) ? "x" : "-");
            // Add to array
            items_p[i] = imode.append(" ")
                         .append(std::to_string(tmp_inode->i_links_count)).append(" ")
                         .append(std::to_string(tmp_inode->i_uid)).append(" ")
                         .append(std::to_string(tmp_inode->i_gid)).append(" ")
                         .append(std::to_string(tmp_inode->i_size)).append(" ")
                         .append(tt).append(" ")
                         .append(dir_entry.name)
                         .append((dir_entry.file_type == 2? "/" : ""));
            delete tmp_inode;
        } else {
            items_p[i] = std::string(dir_entry.name).append(dir_entry.file_type == 2? "/" : "");
        }
    }

    delete dir_inode;

    return items_p;
}

bool fsprogs::write_to_device(const char *data, int b_pos, unsigned long size) {
    // Clear block first
    device_f.seekp(g_block_size * b_pos, std::ios::beg);
    device_f.write(empty_block, g_block_size);
    // Write data
    device_f.seekp(g_block_size * b_pos, std::ios::beg);
    device_f.write(data, size);
    // Flush
    device_f.flush();
    return true;
}
