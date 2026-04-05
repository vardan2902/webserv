#pragma once

#include <map>
#include <vector>
#include <iostream>
#include <ostream>
#include <unistd.h>

#include "ServerException.hpp"
#include "../listener-factory/IListenerFactory.hpp"
#include "../http/request-parser/RequestParser.hpp"
#include "../http/request-parser/RequestParserException.hpp"
#include "../http/router/Router.hpp"
#include "../http/response-manager/ResponseManager.hpp"
#include "IServerManager.hpp"
#include "../logger/ILogger.hpp"
#include "../config.hpp"
#include "../di/DIContainer.hpp"

class ServerManager : public IServerManager {
private:
	static ServerManager* _instance;

	std::vector<Server>&  _servers;
	std::map<int, IListener*>  _fdToListener;
	std::map<int, Server*>  _fdToServer;

	ServerManager(std::vector<Server>&);
	ServerManager(const ServerManager&);
	ServerManager& operator=(const ServerManager&);

public:
	static ServerManager& getInstance(std::vector<Server>&);
	static ServerManager& getInstance();
	static void            destroyInstance();
	virtual ~ServerManager();

	void initializeListeningSockets();
	void registerWithEventLoop();
};
