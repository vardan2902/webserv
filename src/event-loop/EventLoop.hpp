#pragma once

#include <map>
#include <sstream>
#include <sys/epoll.h>
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>

#include "../config.hpp"
#include "../listener/IListener.hpp"
#include "../http/request-parser/RequestParser.hpp"
#include "../http/router/Router.hpp"
#include "../http/response-manager/ResponseManager.hpp"
#include "../http/request-parser/RequestParserException.hpp"
#include "EventLoopException.hpp"

#define BUFFER_SIZE 1024
#define MAX_EVENTS 128

typedef enum State {
	READING,
	WRITING
} EState;

struct Connection {
	int fd;
	size_t bytes_sent;
	EState state;
	std::string in_buffer;
	std::string out_buffer;
	Server* server;
};

class EventLoop {
private:
	static int _epollFd;
	static std::map<int, Server*>* _fdToServer;
	static std::map<int, Connection> _connections;

	static void registerListener(const std::pair<const int, IListener*>&);
	static void _handleAcceptConnection(int);
	static void _closeConnection(int);
	static void _processRequest(Connection&);
	static void _rejectOversizedBody(Connection&);
	static void _resetToReading(Connection&);
	static void _handleRead(Connection&);
	static void _handleWrite(Connection&);
public:
	static void initPoll();
	static void run(std::map<int, Server*>&);
	static void registerListeners(std::map<int, IListener*>&);
};
