#include "ServerManager.hpp"

ServerManager* ServerManager::_instance = NULL;

ServerManager::ServerManager(
	IListenerFactory& listenerFactory,
	std::vector<Server>& servers,
	ILogger& logger
):
	_logger(logger),
	_listenerFactory(listenerFactory),
	_servers(servers)
{}

ServerManager::~ServerManager() {
	for (std::map<int, IListener*>::iterator it = _fdToListener.begin(); it != _fdToListener.end(); ++it) {
		delete it->second;
	}
	_instance = NULL;
}

ServerManager& ServerManager::getInstance(
	IListenerFactory& listenerFactory,
	std::vector<Server>& servers,
	ILogger& logger
) {
	if (_instance == NULL)
		_instance = new ServerManager(listenerFactory, servers, logger);
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
		IListener* listener = _listenerFactory.create(_servers[i].port);
		_fdToListener.insert(std::pair<int, IListener*>(listener->fd(), listener));
		_fdToServer.insert(std::pair<int, Server&>(listener->fd(), _servers[i]));
	}
}

void ServerManager::registerWithEventLoop() {

}
