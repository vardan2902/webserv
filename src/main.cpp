#include "webserv.hpp"
#include "listener-factory/ListenerFactory.hpp"
#include "logger/Logger.hpp"
#include "server-manager/ServerManager.hpp"
#include "http/request-parser/RequestParser.hpp"
#include "http/router/Router.hpp"
#include "http/response-manager/ResponseManager.hpp"
#include "session/SessionManager.hpp"
#include "di/DIContainer.hpp"

std::string readConfigFile(const std::string& path) {
    std::ifstream file(path.c_str());
    if (!file.is_open())
        throw std::runtime_error("Failed to open config file: " + path);

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int main(int argc, char** argv) {
    Logger logger;
    DIContainer::getInstance().bind<ILogger>(DI_LOGGER, logger);

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

        ListenerFactory  listenerFactory;
        RequestParser    requestParser;
        Router           router;
        ResponseManager  responseManager;
        SessionManager   sessionManager;

        DIContainer& di = DIContainer::getInstance();
        di.bind<IListenerFactory>(DI_LISTENER_FACTORY, listenerFactory);
        di.bind<IRequestParser>(DI_REQUEST_PARSER, requestParser);
        di.bind<IRouter>(DI_ROUTER, router);
        di.bind<IResponseManager>(DI_RESPONSE_MANAGER, responseManager);
        di.bind<ISessionManager>(DI_SESSION_MANAGER, sessionManager);

        ServerManager& sm = ServerManager::getInstance(servers);
        sm.initializeListeningSockets();
        sm.registerWithEventLoop();
    } catch (const ValidationException& e) {
        std::string err(e.what());
        logger.error("Validation error: " + err);
        ServerManager::destroyInstance();
        DIContainer::destroyInstance();
        return EXIT_FAILURE;
    } catch (const ParserException& e) {
        std::string err(e.what());
        logger.error("Config parse error: " + err);
        ServerManager::destroyInstance();
        DIContainer::destroyInstance();
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::string err(e.what());
        logger.error("Error: " + err);
        ServerManager::destroyInstance();
        DIContainer::destroyInstance();
        return EXIT_FAILURE;
    }

    ServerManager::destroyInstance();
    DIContainer::destroyInstance();
    return EXIT_SUCCESS;
}
