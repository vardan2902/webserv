#pragma once

#include <map>
#include <sys/epoll.h>
#include <iostream>
#include <unistd.h>

#include "../config.hpp"
#include "../listener/IListener.hpp"
#include "../http/request-parser/RequestParser.hpp"
#include "../http/router/Router.hpp"
#include "../http/response-manager/ResponseManager.hpp"
#include "../http/request-parser/RequestParserException.hpp"
#include "EventLoopException.hpp"

class EventLoop {
private:
	static int _epollFd;
	static void registerListener(const std::pair<const int, IListener*>&);
	static void _handleEvent(const epoll_event& ev, std::map<int, Server*>& fdToServer);
	static std::string _readRequest(int clientFd);
	static void _dispatch(int clientFd, const std::string& raw, Server& server);
public:
	static void initPoll();
	static void run(std::map<int, Server*>& fdToServer);
	static void registerListeners(std::map<int, IListener*>&);
};
