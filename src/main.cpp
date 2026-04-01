#include "webserv.hpp"
#include "listener-factory/ListenerFactory.hpp"
#include "logger/Logger.hpp"
#include "server-manager/ServerManager.hpp"

std::string readConfigFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Failed to open config file: " + path);

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int main(int argc, char** argv) {
    Logger logger;

    if (argc != 2) {
        std::string name(argv[0]);
        logger.error("Usage: " + name + " <config_file>");
        return EXIT_FAILURE;
    }

    try {
        std::string configText = readConfigFile(argv[1]);

        Tokenizer tokenizer(configText);
        tokenizer.tokenize();

        Parser parser(tokenizer);
        parser.parse();

        std::vector<Server> servers = parser.getServers();

        Validator::validate(servers);

        ListenerFactory listenerFactory;
        ServerManager& sm = ServerManager::getInstance(listenerFactory, servers, logger);
        sm.initializeListeningSockets();
        sm.registerWithEventLoop();
    } catch (const ValidationException& e) {
        std::string err(e.what());
        logger.error("Validation error: " + err);
        ServerManager::destroyInstance();
        return EXIT_FAILURE;
    } catch (const ParserException& e) {
        std::string err(e.what());
        logger.error("Config parse error: " + err);
        ServerManager::destroyInstance();
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::string err(e.what());
        logger.error("Error: " + err);
        ServerManager::destroyInstance();
        return EXIT_FAILURE;
    }

    ServerManager::destroyInstance();
    return EXIT_SUCCESS;
}
