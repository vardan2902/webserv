#pragma once
#include <string>
#include <vector>
#include <map>

#define DEFAULT_PORT 80

struct Location {
    std::string path;
    std::string root;
    std::string index;
    std::map<std::string, std::string> cgiHandlers; // extension -> interpreter path
};

struct Server {
    int port = DEFAULT_PORT;
    std::string root;
    std::vector<Location> locations;
    std::map<int, std::string> errorPages; // status code -> file path relative to root
};
