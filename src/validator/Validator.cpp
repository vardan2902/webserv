#include "Validator.hpp"
#include <sstream>

void Validator::validate(const std::vector<Server>& servers) {
    std::set<std::string> usedBinds;
    for (size_t i = 0; i < servers.size(); ++i) {
        const Server& server = servers[i];
        if (server.port < 1 || server.port > 65535) {
            std::ostringstream oss; oss << server.port;
            throw ValidationException("Invalid port: " + oss.str());
        }
        std::ostringstream bindKey;
        bindKey << server.host << ":" << server.port;
        if (!usedBinds.insert(bindKey.str()).second)
            throw ValidationException("Duplicate listen address: " + bindKey.str());
        if (server.root.empty())
            throw ValidationException("Server root cannot be empty");

        std::set<std::string> locationPaths;
        for (size_t j = 0; j < server.locations.size(); ++j) {
            const Location& loc = server.locations[j];
            if (loc.path.empty() || loc.path[0] != '/')
                throw ValidationException("Location path must start with '/'");
            if (!locationPaths.insert(loc.path).second)
                throw ValidationException("Duplicate location path: " + loc.path);
            if (loc.root.empty() && server.root.empty())
                throw ValidationException("Location root cannot be empty when server root is not set");
            if (loc.returnCode != 0 && (loc.returnCode < 300 || loc.returnCode > 399))
                throw ValidationException("Return code must be in range 300-399");
        }
    }
}
