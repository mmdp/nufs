#include <iostream>
#include <vector>
#include <map>
#include <algorithm>
#include <libgen.h>
#include "mkfs.h"

using std::string;

inline void print(string msg) {
    std::cout << msg << std::endl;
}

inline string mk_abs_path(string pwd, string path);

std::vector<string> split_cmd(string cmd);

int main(int argc, char **argv) {

    string device_file_name;
    uint16_t data_blocks_count;
    
    // Input device size
    //cin >> device_file_name;
    //cin >> data_blocks_count;
    device_file_name = "test.nufs";
    data_blocks_count = 4096;

    // Create device file
    create_device_file(device_file_name, data_blocks_count);
    
    // Create fs
    fsprogs *fsp = new fsprogs(device_file_name);
    fsp->create_fs(data_blocks_count);
    
    // Create base system tree
    create_base_tree(fsp);

    // Create user & group
    std::map<int, string> users;
    users[0] = "root";
    std::map<int, string> groups;
    groups[0] = "root";

    // Current env
    string pwd = "/";
    uint16_t current_uid = 0;
    uint16_t current_gid = 0;
    uint16_t umask = 0022;
    Inode* pwd_inode = new Inode;
    Inode* tmp_inode = fsp->get_last_inode(pwd);
    if (tmp_inode != nullptr) {
        pwd_inode = tmp_inode;
    }

    // main loop
    char input[256];
    while (true) {
        std::cout << users[current_uid] << "@" << pwd << "$ ";
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
                        if (args_pos != string::npos) {
                            path = path.substr(2+args_pos);
                        }
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
                path = mk_abs_path(pwd, path);
            }
            string* items = fsp->get_items(path, show_detail);
            if (items == nullptr) {
                print(string("ls: cannot access ").append(path).append(": No such file or directory"));
                continue;
            }
            for (int i=0; i<16; i++) {
                if (items[i] != "") {
                    if (show_detail) {
                        // Replace uid & gid
                        unsigned long s1 = items[i].find_first_of(' ');
                        unsigned long s2 = s1 + 1 + items[i].substr(s1+1).find_first_of(' ');
                        unsigned long s3 = s2 + 1 + items[i].substr(s2+1).find_first_of(' ');
                        unsigned long s4 = s3 + 1 + items[i].substr(s3+1).find_first_of(' ');
                        string user = users[std::stoi(items[i].substr(s2+1, s3-s2-1))];
                        string group = groups[std::stoi(items[i].substr(s3+1, s4-s3-1))];
                        if (group != "") {
                            items[i].replace(s3+1, s4-s3-1, group);
                        }
                        if (user != "") {
                            items[i].replace(s2+1, s3-s2-1, user);
                        }
                    }
                    print(items[i]);
                }
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
                string path = mk_abs_path(pwd, cmd.substr(2+args_pos));
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
                    print(string("cd: The directory “").append(path).append("” does not exist"));
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
            path = mk_abs_path(pwd, path.substr(path_pos));
            int i_pos = fsp->get_last_inode_pos(path);
            if (i_pos < 0) {
                print(string("chmod: cannot access ").append(path).append(": No such file or directory"));
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

            string path = mk_abs_path(pwd, tokens[2]);
            int i_pos = fsp->get_last_inode_pos(path);
            if (i_pos < 0) {
                print(string("chmod: cannot access ").append(path).append(": No such file or directory"));
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
            // TODO gid name
            uint16_t gid = (uint16_t) std::stoi(tokens[1]);

            string path = mk_abs_path(pwd, tokens[2]);
            int i_pos = fsp->get_last_inode_pos(path);
            if (i_pos < 0) {
                print(string("chmod: cannot access ").append(path).append(": No such file or directory"));
                continue;
            }
            tmp_inode = fsp->read_inode((uint16_t) i_pos);

            tmp_inode->i_gid = gid;
            fsp->update_inode((uint16_t) i_pos, tmp_inode);

            delete tmp_inode;
        }



        // Directory
        if (cmd.substr(0, 6) == "mkdir ") {
            std::vector<string> tokens = split_cmd(cmd);
            if (tokens.size() != 2) {
                print("Invalid arguments.");
            }
            string path = tokens[1];
            while (path[path.size()-1] == '/') {
                path = path.substr(0, path.size()-1);
                if (path == "") {
                    print("Invalid path.");
                    continue;
                }
            }
            path = mk_abs_path(pwd, path);
            unsigned long last_d = path.find_last_of('/');
            string dir_name = path.substr(last_d+1);
            path = path.substr(0, path.size()-dir_name.size()-1);
            log("dir name", dir_name);
            log("path", path);
            log("curr uid", current_uid);
            if (!fsp->create_directory(dir_name, path, current_uid, current_gid)) {
                print(string("mkdir: cannot create directory ‘").append(tokens[1]).append("’"));
                continue;
            }
        }

        if (cmd.substr(0, 6) == "rmdir ") {
            std::vector<string> tokens = split_cmd(cmd);
            if (tokens.size() != 2) {
                print("Invalid arguments.");
                continue;
            }
            string path = tokens[1];
            while (path[path.size()-1] == '/') {
                path = path.substr(0, path.size()-1);
                if (path == "") {
                    print("Invalid path.");
                    continue;
                }
            }
            path = mk_abs_path(pwd, path);
            if (!fsp->remove_dir(path)) {
                print("Remove failed. Not empty.");
                continue;
            }
        }


        // File
        if (cmd.substr(0, 3) == "ln ") {
            // TODO soft link

            std::vector<string> tokens = split_cmd(cmd);
            if (tokens.size() != 3) {
                print("Invalid arguments.");
                continue;
            }

            string file_path = mk_abs_path(pwd, tokens[1]);
            if (file_path == "/") {
                print("Cannot hardlink /");
                continue;
            }
            string dest_path = mk_abs_path(pwd, tokens[2]);

            // TODO to file
            if (!fsp->add_link(file_path, dest_path)) {
                print("Link failed.");
                continue;
            }
        }

        if (cmd.substr(0, 3) == "mv ") {
            // link first
            std::vector<string> tokens = split_cmd(cmd);
            if (tokens.size() != 3) {
                print("Invalid arguments.");
                continue;
            }

            string file_path = mk_abs_path(pwd, tokens[1]);
            if (file_path == "/") {
                print("Cannot move /");
                continue;
            }
            string dest_path = mk_abs_path(pwd, tokens[2]);

            if (!fsp->add_link(file_path, dest_path)) {
                print("Move failed.");
                continue;
            }

            // then remove
            if (!fsp->remove_file(file_path)) {
                print("Remove failed.");
                continue;
            }
        }

        if (cmd.substr(0, 3) == "rm ") {
            std::vector<string> tokens = split_cmd(cmd);
            if (tokens.size() != 2) {
                print("Invalid arguments.");
                continue;
            }

            string file_path = mk_abs_path(pwd, tokens[1]);

            // TODO disable rm empty dir
            if (!fsp->remove_file(file_path)) {
                print("Remove failed.");
                continue;
            }
        }

        if (cmd.substr(0, 6) == "touch ") {
            std::vector<string> tokens = split_cmd(cmd);
            if (tokens.size() != 2) {
                print("Invalid arguments.");
                continue;
            }
            string file_path = mk_abs_path(pwd, tokens[1]);
            string file_name = basename((char *) file_path.c_str());
            string path = dirname(strdup((char *) file_path.c_str()));

            fsp->touch_file(file_name, path, current_uid, current_gid);
        }

        if (cmd.substr(0, 5) == "echo ") {
            std::vector<string> tokens = split_cmd(cmd);

            string data = "";
            if (std::find(tokens.begin(), tokens.end(), ">") != tokens.end()) {
                string file_path;
                int i = 1;
                for (; i < tokens.size(); i++) {
                    if (tokens[i] != ">")
                        data += tokens[i].append(" ");
                    else
                        break;
                }
                if (tokens.size() == i + 2) {
                    file_path = mk_abs_path(pwd, tokens[i+1]);
                    if (data == "") {
                        print("Not supported yet.");
                    }
                    string file_name = basename((char *) file_path.c_str());
                    string dir_path = dirname(strdup((char *) file_path.c_str()));
                    // FIXME once orz!!
                    if (fsp->touch_file(file_name, dir_path, current_uid, current_gid)) {
                        fsp->write_to_file(file_path, data);
                    }
                } else {
                    print("Invalid arguments.");
                    continue;
                }
            } else {
                for (int i = 1; i < tokens.size()-1; i++) {
                        data += tokens[i].append(" ");
                }
                data += tokens[tokens.size()-1];
                print(data);
            }

        }

        if (cmd.substr(0, 3) == "cp ") {
            std::vector<string> tokens = split_cmd(cmd);
            if (tokens.size() != 3) {
                print("Invalid arguments.");
                continue;
            }

            string file_path = mk_abs_path(pwd, tokens[1]);
            if (file_path == "/") {
                print("Cannot copy /");
                continue;
            }
            string dest_path = mk_abs_path(pwd, tokens[2]);
            string data = fsp->read_file(file_path);
            print(data);
            string file_name = basename((char *) dest_path.c_str());
            string dir_path = dirname(strdup((char *) dest_path.c_str()));
            print(dir_path);
            print(file_name);
            print(dest_path);
            if (fsp->touch_file(file_name, dir_path, current_uid, current_gid)) {
                if (fsp->write_to_file(dest_path, data)) {
                    continue;
                } else {
                    print("Write failed.");
                }
            } else {
                print("Touch failed.");
            }
        }

        if (cmd.substr(0, 4) == "cat ") {
            std::vector<string> tokens = split_cmd(cmd);
            if (tokens.size() < 2) {
                print("Invalid arguments.");
                continue;
            }
            string file_path = mk_abs_path(pwd, tokens[1]);
            print(fsp->read_file(file_path));
        }


        if (cmd.substr(0, 5) == "umask") {
            std::vector<string> tokens = split_cmd(cmd);
            if (tokens[0] != "umask" || tokens.size() > 2) {
                print("Invalid arguments.");
                continue;
            }

            if (tokens.size() == 2) {
                uint16_t nmask = (uint16_t) std::stoi(tokens[1], 0, 8);
                umask = nmask;
            } else {
                string smask = "";
                int _umask = umask;
                while (_umask > 0) {
                    smask = std::to_string(umask % 8).append(smask);
                    _umask /= 8;
                }
                while (smask.size() < 4) {
                    smask = string("0").append(smask);
                }
                print(smask);
            }

        }

        if (cmd == "passwd") {
            print("Permission denied.");
        }

        if (cmd == "help") {
            print("No help available!");
        }

    }

    return 0;
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

inline string mk_abs_path(string pwd, string path) {
    while (path[path.size()-1] == '/') {
        path = path.substr(0, path.size() - 1);
        if (path == "/") break;
    }
    if (path[0] != '/') {
        if (pwd[pwd.size()-1] == '/') {
            return pwd.append(path);
        }
        return pwd.append("/").append(path);
    } else {
        return path;
    }
}


