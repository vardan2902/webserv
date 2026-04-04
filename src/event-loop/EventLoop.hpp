#pragma once

#include <map>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <cerrno>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/socket.h>

#include "../config.hpp"
#include "../listener/IListener.hpp"
#include "../http/request-parser/IRequestParser.hpp"
#include "../http/router/IRouter.hpp"
#include "../http/response-manager/IResponseManager.hpp"
#include "../http/request-parser/RequestParserException.hpp"
#include "../logger/ILogger.hpp"
#include "../di/DIContainer.hpp"
#include "EventLoopException.hpp"

#define BUFFER_SIZE       1024
#define MAX_EVENTS        128
#define IDLE_TIMEOUT_SECS 60
#define EPOLL_TIMEOUT_MS  5000

typedef enum State {
	READING,
	WRITING
} EState;

struct Connection {
	int         fd;
	size_t      bytes_sent;
	EState      state;
	std::string in_buffer;
	std::string out_buffer;
	Server*     server;
	std::string clientIp;
	int         clientPort;
	time_t      lastActivity;
};

class EventLoop {
private:
	static int                       _epollFd;
	static std::map<int, Server*>*   _fdToServer;
	static std::map<int, Connection> _connections;
	static ILogger*                  _logger;

	static void        registerListener(const std::pair<const int, IListener*>&);
	static void        _handleAcceptConnection(int);
	static void        _closeConnection(int);
	static void        _processRequest(Connection&);
	static void        _prepareWrite(Connection&);
	static void        _rejectOversizedBody(Connection&);
	static void        _resetToReading(Connection&);
	static void        _handleRead(Connection&);
	static void        _handleWrite(Connection&);
	static void        _logAccess(const Connection&, const HttpRequest&, int, size_t);
	static void        _logResponse(const Connection&, const HttpRequest&);
	static void        _sweepIdleConnections();
	static std::string _itoa(int);
public:
	static void initPoll();
	static void run(std::map<int, Server*>&);
	static void registerListeners(std::map<int, IListener*>&);
};
