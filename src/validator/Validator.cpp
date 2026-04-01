#include "Validator.hpp"

void Validator::validate(const std::vector<Server>& servers) {
    std::set<int> usedPorts;
    for (size_t i = 0; i < servers.size(); ++i) {
        const Server& server = servers[i];
        if (server.port < 1 || server.port > 65535)
            throw ValidationException("Invalid port: " + std::to_string(server.port));
        if (!usedPorts.insert(server.port).second)
            throw ValidationException("Duplicate port: " + std::to_string(server.port));
        if (server.root.empty())
            throw ValidationException("Server root cannot be empty");

        std::set<std::string> locationPaths;
        for (size_t j = 0; j < server.locations.size(); ++j) {
            const Location& loc = server.locations[j];
            if (loc.path.empty() || loc.path[0] != '/')
                throw ValidationException("Location path must start with '/'");
            if (!locationPaths.insert(loc.path).second)
                throw ValidationException("Duplicate location path: " + loc.path);
            if (loc.root.empty())
                throw ValidationException("Location root cannot be empty");
            if (loc.index.empty() && loc.cgiHandlers.empty())
                throw ValidationException("Location index cannot be empty");
        }
    }
}
