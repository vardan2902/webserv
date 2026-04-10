#pragma once

#include <map>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <cerrno>
#include <csignal>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/socket.h>

#include "../config.hpp"
#include "../listener/IListener.hpp"
#include "../http/request-parser/IRequestParser.hpp"
#include "../http/router/IRouter.hpp"
#include "../http/response-manager/IResponseManager.hpp"
#include "../http/request-parser/RequestParserException.hpp"
#include "../http/cgi/CgiHandler.hpp"
#include "../http/cgi/CgiException.hpp"
#include "../logger/ILogger.hpp"
#include "../di/DIContainer.hpp"
#include "../listener-factory/utils/ListenerUtils.hpp"

#include "EventLoopException.hpp"

#define BUFFER_SIZE       65536
#define MAX_EVENTS        128
#define IDLE_TIMEOUT_SECS 60
#define EPOLL_TIMEOUT_MS  5000
#define CGI_TIMEOUT_SECS  60

typedef enum State {
	READING,
	WRITING,
	CGI_WRITING,
	CGI_READING
} EState;

struct CgiContext {
	pid_t       pid;
	int         stdinFd;
	int         stdoutFd;
	std::string inputBuf;
	size_t      inputOffset;
	std::string outputBuf;
	time_t      startTime;
	HttpRequest req;

	CgiContext() : pid(-1), stdinFd(-1), stdoutFd(-1), inputOffset(0), startTime(0) {}
};

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
	CgiContext  cgi;

	// Body-completion tracking (avoids O(N²) re-scan on every read event)
	size_t      headerEndCache;  // 0 = headers not yet complete; >0 = body start offset
	ssize_t     contentLength;   // -2 = unknown, -1 = chunked, 0 = no body, >0 = expected size
	size_t      searchPos;       // for chunked: resume find("0\r\n\r\n") from here

	Connection()
		: fd(-1), bytes_sent(0), state(READING),
		  server(NULL), clientPort(0), lastActivity(0),
		  headerEndCache(0), contentLength(-2), searchPos(0) {}
};

class EventLoop {
private:
	static int                       _epollFd;
	static std::map<int, Server*>*   _fdToServer;
	static std::map<int, Connection> _connections;
	static std::map<int, int>        _cgiToConn;  // cgi pipe fd → client fd
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

	static bool        _tryDispatchCgi(Connection&, HttpRequest&,
	                                   const std::string& pathOnly,
	                                   const std::string& queryString,
	                                   const Location*);
	static void        _startCgi(Connection&, HttpRequest&,
	                              const Location*, const std::string& filePath,
	                              const std::string& queryString);
	static void        _initCgiContext(Connection&, HttpRequest&,
	                                   const CgiHandler::CgiProcess&);
	static void        _registerCgiPipes(Connection&, const CgiHandler::CgiProcess&);
	static void        _handleCgi(int fd, uint32_t events);
	static void        _handleCgiWrite(Connection&);
	static void        _handleCgiRead(Connection&);
	static void        _cleanupCgi(Connection&);
	static void        _abortCgi(Connection&, int statusCode);

public:
	static void initPoll();
	static void run(std::map<int, Server*>&);
	static void registerListeners(std::map<int, IListener*>&);
};
