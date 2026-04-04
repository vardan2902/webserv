#include "ServerManager.hpp"

#include "event-loop/EventLoop.hpp"

ServerManager* ServerManager::_instance = NULL;

ServerManager::ServerManager(std::vector<Server>& servers):
	_servers(servers)
{}

ServerManager::~ServerManager() {
	for (std::map<int, IListener*>::iterator it = _fdToListener.begin(); it != _fdToListener.end(); ++it) {
		delete it->second;
	}
	_instance = NULL;
}

ServerManager& ServerManager::getInstance(std::vector<Server>& servers) {
	if (_instance == NULL)
		_instance = new ServerManager(servers);
	return *_instance;
}

void ServerManager::destroyInstance() {
	if (_instance != NULL)
		delete _instance;
}

ServerManager& ServerManager::getInstance() {
	if (_instance == NULL)
		throw ServerException("ServerManager is not initialized");
	return *_instance;
}

void ServerManager::initializeListeningSockets() {
	for (size_t i = 0; i < _servers.size(); ++i) {
		IListener* listener = DIContainer::getInstance().resolve<IListenerFactory>(DI_LISTENER_FACTORY).create(_servers[i].host, _servers[i].port);
		_fdToListener.insert(std::pair<int, IListener*>(listener->fd(), listener));
		_fdToServer.insert(std::pair<int, Server*>(listener->fd(), &_servers[i]));

		std::ostringstream msg;
		msg << "server listening on " << _servers[i].host << ":" << _servers[i].port;
		DIContainer::getInstance().resolve<ILogger>(DI_LOGGER).info(msg.str());
	}
}

void ServerManager::registerWithEventLoop() {
	ILogger& logger = DIContainer::getInstance().resolve<ILogger>(DI_LOGGER);
	try {
		EventLoop::initPoll();
		EventLoop::registerListeners(_fdToListener);
		EventLoop::run(_fdToServer);
	} catch (EventLoopException& e) {
		logger.error(std::string("EventLoop error: ") + e.what());
	} catch (ServerException& e) {
		logger.error(std::string("Server error: ") + e.what());
	} catch (RequestParserException& e) {
		logger.error(std::string("Request parser error: ") + e.what());
	} catch (std::exception& e) {
		logger.error(std::string("Unexpected error: ") + e.what());
	}
}
