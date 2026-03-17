#pragma once
#include <string>
#include <vector>

#define DEFAULT_PORT 80

struct Location
{
    std::string path;
    std::string root;
    std::string index;
};

struct Server
{
    int port = DEFAULT_PORT;
    std::string root;
    std::vector<Location> locations;
};