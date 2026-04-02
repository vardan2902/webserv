#pragma once

#include <map>
#include <string>
#include <vector>

#define DEFAULT_PORT 80

struct Location {
    bool                        autoindex;
    int                         returnCode;
    std::string                 returnUrl;
    std::string                 path;
    std::string                 root;
    std::string                 index;
    std::vector<std::string>    allowMethods;
    Location() : autoindex(false), returnCode(0) {}
};

struct Server {
    int                         port;
    size_t                      clientMaxBodySize;
    std::string                 root;
    std::vector<Location>       locations;
    std::map<int, std::string>  errorPages;
    Server() : port(DEFAULT_PORT), clientMaxBodySize(0) {}
};
