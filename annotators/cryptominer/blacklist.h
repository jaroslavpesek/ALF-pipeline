#ifndef BLACKLIST_H_
#define BLACKLIST_H_

#include <unordered_set>
#include <unirec/unirec.h>

#include "xxhash.h"

struct filter_pair {
    ip_addr_t ip;
    uint16_t port;

    filter_pair(ip_addr_t ip, uint16_t port) : 
        ip(ip), port(port) 
    {
        
    }

} __attribute__ ((packed));

// Hash function definiton for ip_addr_t data type.
namespace std
{
    template<>
    struct hash<filter_pair>
    {
        std::size_t operator()(const filter_pair& pair) const 
        {
            return static_cast<std::size_t>(XXH3_64bits(&pair, sizeof(pair)));
        }
    };
}

inline bool operator==(const filter_pair& rhs, const filter_pair& lhs)
{
    int cmp = ip_cmp(&rhs.ip, &lhs.ip);
    return cmp == 0 && rhs.port == lhs.port ? true : false;
}

class Blacklist {
    std::unordered_set<filter_pair> blacklist;

public:

    int load_blacklist(const std::string& filename);

    bool is_blacklisted(const filter_pair& in_pair);
};

#endif /* BLACKLIST_H_ */