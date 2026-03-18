#include "webserv.hpp"

std::string readConfigFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Failed to open config file: " + path);

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void printServerConfig(const std::vector<Server>& servers) {
    for (const auto& server : servers) {
        std::cout << "Server port: " << server.port
                  << " root: " << server.root << "\n";

        for (const auto& loc : server.locations) {
            std::cout << "  Location " << loc.path
                      << " root: " << loc.root
                      << " index: " << loc.index << "\n";
        }
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>\n";
        return 1;
    }

    try {
        std::string configText = readConfigFile(argv[1]);

        Tokenizer tokenizer(configText);
        tokenizer.tokenize();

        Parser parser(tokenizer);
        parser.parse();

        Validator::validate(parser.getServers());

        printServerConfig(parser.getServers());
    } catch (const ValidationException& e) {
        std::cerr << "Validation error: " << e.what() << "\n";
        return EXIT_FAILURE;
    } catch (const ParserException& e) {
        std::cerr << "Config parse error: " << e.what() << "\n";
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return 0;
}
