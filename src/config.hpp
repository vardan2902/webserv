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
    std::string                 uploadStore;
    std::map<std::string, std::string> cgiExtensions; // ".py" → "/usr/bin/python3"
    Location() : autoindex(false), returnCode(0) {}
};

struct Server {
    int                         port;
    size_t                      clientMaxBodySize;
    std::string                 host;
    std::string                 root;
    std::vector<Location>       locations;
    std::map<int, std::string>  errorPages;
    Server() : port(DEFAULT_PORT), clientMaxBodySize(0), host("0.0.0.0") {}
};
