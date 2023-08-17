#include <fstream>
#include <iostream>

#include "blacklist.h"

int 
Blacklist::load_blacklist(const std::string& filename)
{
    std::string str_ip;
    uint16_t port;
    std::ifstream blacklist_file(filename);

    while (blacklist_file >> str_ip >> port) {
        ip_addr_t ip;
        if (ip_from_str(str_ip.c_str(), &ip) == 0) {
            std::cerr << "Invalid blacklist IP address \"" << str_ip << "\". Skipping..." << std::endl;
            continue;
        }
        
        blacklist.emplace(ip, port);
    }
    return 0;
}

bool 
Blacklist::is_blacklisted(const filter_pair& in_pair)
{
    return blacklist.find(in_pair) == blacklist.end() ? false : true;
}