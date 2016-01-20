#include <iostream>
#include <vector>
#include "mkfs.h"

using std::string;

inline void print(string msg) {
    std::cout << msg << std::endl;
}

std::vector<string> split_cmd(string cmd);

int main(int argc, char **argv) {

    string file_name;
    uint16_t file_size;
    
    // Input device size
    //cin >> file_name;
    //cin >> file_size;
    file_name = "test.ncfs";
    file_size = 4096;

    // Create device file
    create_device_file(file_name, file_size);
    
    // Create fs
    fsprogs *fsp = new fsprogs(file_name);
    fsp->create_fs(file_size);
    
    // Create base system tree
    create_base_tree(fsp);

    // main loop
    char input[256];
    string pwd = "/root";
    string user = "root";
    string group = "root";
    Inode* pwd_inode;
    Inode* tmp_inode = fsp->get_last_inode(pwd);
    if (tmp_inode != nullptr) {
        pwd_inode = tmp_inode;
    }
    while (true) {
        std::cout << user << "@" << pwd << "$ ";
        std::cin.getline(input, 255);
        string cmd(input);
        if (cmd == "x" || cmd == "exit" || cmd == "quit")
            break;

        if (cmd.substr(0, 3) == "pwd") {
            print(pwd);
        }

        if (cmd.substr(0, 2) == "ls") {
            if (cmd.size() > 2 && cmd[2] != ' ') {
                print("No such command.");
                continue;
            }
            bool show_detail = false;
            string path = pwd;
            if (cmd.size() > 3) {
                unsigned long args_pos = cmd.substr(2).find_first_not_of(' ');
                path = cmd.substr(2 + args_pos);
                if (path[0] == '-') {
                    if (path[1] == 'l' && (path.size() == 2 || path[2] == ' ')) {
                        args_pos = path.substr(2).find_first_not_of(' ');
                        if (args_pos != string::npos)
                            path = path.substr(2+args_pos);
                        else
                            path = pwd;
                        show_detail = true;
                    } else {
                        print("Invalid option.");
                        continue;
                    }
                }
                if (path.find_first_of(' ') != string::npos) {
                    print("Invalid argument.");
                    continue;
                }
            }
            string* items = fsp->get_items(path, show_detail);
            if (items == nullptr) {
                print(string("ls: cannot access ") + path + string(": No such file or directory"));
                continue;
            }
            for (int i=0; i<16; i++) {
                if (items[i] != "")
                    print(items[i]);
            }
        }

        if (cmd.substr(0, 2) == "cd") {
            if (cmd.size() == 2) {
                // TODO goto home
                continue;
            }
            if (cmd[2] != ' ') {
                print("No such command.");
                continue;
            }
            if (cmd.size() > 3) {
                unsigned long args_pos = cmd.substr(2).find_first_not_of(' ');
                string path = cmd.substr(2+args_pos);
                if (path.find_first_of(' ') != string::npos) {
                    print("Invalid argument.");
                    continue;
                }
                tmp_inode = fsp->get_last_inode(path);
                if (tmp_inode != nullptr) {
                    delete pwd_inode;
                    pwd_inode = tmp_inode;
                    pwd = path;
                }
                else {
                    print(string("cd: The directory “") + path + string("” does not exist"));
                    continue;
                }
            }
        }

        // Change Inode

        if (cmd.substr(0, 5) == "chmod") {
            if (cmd.size() < 11 || cmd[5] != ' ') {
                print("Syntax error.");
                continue;
            }
            unsigned long args_pos = cmd.substr(5).find_first_not_of(' ');
            string args = cmd.substr(5+args_pos, 3);
            uint16_t imode = (uint16_t) std::stoi(args, 0, 8);

            string path = cmd.substr(8+args_pos);
            unsigned long path_pos = path.find_first_not_of(' ');
            path = path.substr(path_pos);
            int i_pos = fsp->get_last_inode_pos(path);
            if (i_pos < 0) {
                print(string("chmod: cannot access ") + path + string(": No such file or directory"));
                continue;
            }
            tmp_inode = fsp->read_inode((uint16_t) i_pos);

            tmp_inode->i_mode = (uint16_t) (tmp_inode->i_mode & 65024) | imode;
            fsp->update_inode((uint16_t) i_pos, tmp_inode);
            delete tmp_inode;
        }

        if (cmd.substr(0, 5) == "chown") {
            if (cmd.size() < 9 || cmd[5] != ' ') {
                print("Syntax error.");
                continue;
            }
            std::vector<string> tokens = split_cmd(cmd);
            if (tokens.size() != 3) {
                print("Invalid arguments.");
            }
            uint16_t uid = (uint16_t) std::stoi(tokens[1]);

            string path = tokens[2];
            int i_pos = fsp->get_last_inode_pos(path);
            if (i_pos < 0) {
                print(string("chmod: cannot access ") + path + string(": No such file or directory"));
                continue;
            }
            tmp_inode = fsp->read_inode((uint16_t) i_pos);

            tmp_inode->i_uid = uid;
            fsp->update_inode((uint16_t) i_pos, tmp_inode);

            delete tmp_inode;
        }

        if (cmd.substr(0, 5) == "chgrp") {
            if (cmd.size() < 9 || cmd[5] != ' ') {
                print("Syntax error.");
                continue;
            }
            std::vector<string> tokens = split_cmd(cmd);
            if (tokens.size() != 3) {
                print("Invalid arguments.");
            }
            uint16_t gid = (uint16_t) std::stoi(tokens[1]);

            string path = tokens[2];
            int i_pos = fsp->get_last_inode_pos(path);
            if (i_pos < 0) {
                print(string("chmod: cannot access ") + path + string(": No such file or directory"));
                continue;
            }
            tmp_inode = fsp->read_inode((uint16_t) i_pos);

            tmp_inode->i_gid = gid;
            fsp->update_inode((uint16_t) i_pos, tmp_inode);

            delete tmp_inode;
        }



        //
        if (cmd == "mkdir") {
        }

        if (cmd == "rmdir") {
        }

        if (cmd == "mv") {
        }

        if (cmd == "cp") {
        }

        if (cmd == "rm") {
        }

        if (cmd == "ln") {
        }

        if (cmd == "cat") {
        }


        if (cmd == "umask") {
        }

        if (cmd == "passwd") {
        }

        if (cmd == "help") {
        }

    }

    return 0;
}


bool check_cmd_args(string cmd) {
    unsigned long args_pos = cmd.substr(2).find_first_not_of(' ');
    string path = cmd.substr(2 + args_pos);
    if (path.find_first_of(' ') != string::npos) {
        print("Invalid argument.");
        return false;
    }
}

std::vector<string> split_cmd(string cmd) {
    std::vector<string> tokens;
    char* str = (char *) cmd.c_str();
    char* pch = strtok (str," ");
    while (pch != NULL)
    {
        tokens.push_back(pch);
        pch = strtok (NULL, " ");
    }
    return tokens;
}

