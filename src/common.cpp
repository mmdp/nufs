#include <iostream>

void log(int type_id, std::string msg) {
    std::string type_name;
    switch (type_id) {
        case 1:
            type_name = "ERROR";
            break;
        case 2:
            type_name = "WARNING";
            break;
        case 3:
            type_name = "INFO";
            break;
        default:
            std::cout << "DEBUG: " << type_id << " " << msg << std::endl;
            return;
    }

    std::cout << type_name << ": " << msg << std::endl;
}

void log(std::string key, int value) {
    std::cout << key << ": " << value << std::endl;
}

void log(std::string key, std::string value) {
    std::cout << key << ": " << value << std::endl;
}

int power2(int x) {
    return (x ? 2 * power2(x-1) : 1);
}

