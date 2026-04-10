#include "EventLoop.hpp"

std::string EventLoop::_itoa(int n) {
	std::ostringstream oss;
	oss << n;
	return oss.str();
}

int                       EventLoop::_epollFd    = -1;
std::map<int, Server*>*   EventLoop::_fdToServer = NULL;
std::map<int, Connection> EventLoop::_connections;
std::map<int, int>        EventLoop::_cgiToConn;
ILogger*                  EventLoop::_logger     = NULL;
volatile sig_atomic_t     EventLoop::_running    = 0;

void EventLoop::_signalHandler(int sig) {
	(void)sig;
	_running = 0;
}

void EventLoop::initPoll() {
	_epollFd = epoll_create(1);
	if (_epollFd == -1)
		throw EventLoopException("epoll_create() failed");

	_running = 1;

	signal(SIGINT,  _signalHandler);  // graceful shutdown on Ctrl+C
	signal(SIGTERM, _signalHandler);  // graceful shutdown on kill
	signal(SIGCHLD, SIG_IGN);        // auto-reap CGI child processes
	signal(SIGPIPE, SIG_IGN);        // ignore broken pipe on CGI stdin write
}

void EventLoop::registerListener(const std::pair<const int, IListener *>& socket) {
	int fd = socket.first;
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = fd;
	if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, fd, &ev) == -1)
		throw EventLoopException("epoll_ctl() failed");
}

void EventLoop::registerListeners(std::map<int, IListener *>& fdToListener) {
	for (std::map<int, IListener*>::const_iterator it = fdToListener.begin(); it != fdToListener.end(); ++it)
		EventLoop::registerListener(*it);
}

static std::string _formatIp(const struct sockaddr_in& addr) {
	const unsigned char* b = reinterpret_cast<const unsigned char*>(&addr.sin_addr.s_addr);
	std::ostringstream oss;
	oss << static_cast<int>(b[0]) << "." << static_cast<int>(b[1]) << "."
	    << static_cast<int>(b[2]) << "." << static_cast<int>(b[3]);
	return oss.str();
}

void EventLoop::_handleAcceptConnection(int fd) {
	while (true) {
		try {
			struct sockaddr_in clientAddr;
			socklen_t addrLen = sizeof(clientAddr);
			int client_fd = ::accept(fd, reinterpret_cast<struct sockaddr*>(&clientAddr), &addrLen);
			if (client_fd < 0)
				return;

			ListenerUtils::setNonBlocking(client_fd);

			epoll_event ev;
			ev.events = EPOLLIN;
			ev.data.fd = client_fd;
			epoll_ctl(_epollFd, EPOLL_CTL_ADD, client_fd, &ev);

			Connection conn;
			conn.fd           = client_fd;
			conn.state        = READING;
			conn.bytes_sent   = 0;
			conn.server       = (*_fdToServer)[fd];
			conn.clientIp     = _formatIp(clientAddr);
			conn.clientPort   = ntohs(clientAddr.sin_port);
			conn.lastActivity = time(NULL);

			_connections[client_fd] = conn;

			std::ostringstream msg;
			msg << "accepted connection from " << conn.clientIp << ":" << conn.clientPort;
			_logger->debug(msg.str());
		} catch (const EventLoopException& e) {
			_logger->error(e.what());
		}
	}
}

// Parses Content-Length or detects chunked from the header block (called once per request).
static void _parseBodyMeta(Connection& conn) {
	const std::string& buf = conn.in_buffer;
	size_t headerEnd = conn.headerEndCache;

	const std::string clKey = "Content-Length: ";
	size_t clPos = buf.find(clKey, 0);
	if (clPos != std::string::npos && clPos < headerEnd) {
		size_t valueStart = clPos + clKey.size();
		size_t valueEnd   = buf.find("\r\n", valueStart);
		if (valueEnd != std::string::npos && valueEnd < headerEnd) {
			std::istringstream iss(buf.substr(valueStart, valueEnd - valueStart));
			ssize_t cl = 0;
			iss >> cl;
			conn.contentLength = cl;
			return;
		}
	}

	const std::string teKey = "Transfer-Encoding: chunked";
	size_t tePos = buf.find(teKey, 0);
	if (tePos != std::string::npos && tePos < headerEnd) {
		conn.contentLength = -1;  // chunked
		return;
	}

	conn.contentLength = 0;  // no body
}

void EventLoop::_closeConnection(int fd) {
	std::ostringstream msg;
	msg << "connection closed fd=" << fd;
	_logger->debug(msg.str());

	std::map<int, Connection>::iterator it = _connections.find(fd);
	if (it != _connections.end()) {
		Connection& conn = it->second;
		if (conn.cgi.pid > 0 || conn.cgi.stdinFd >= 0 || conn.cgi.stdoutFd >= 0) {
			if (conn.cgi.pid > 0)
				kill(conn.cgi.pid, SIGKILL);
			_cleanupCgi(conn);
		}
	}

	epoll_ctl(_epollFd, EPOLL_CTL_DEL, fd, NULL);
	close(fd);
	_connections.erase(fd);
}

void EventLoop::_handleRead(Connection& conn) {
	conn.lastActivity = time(NULL);
	char buf[BUFFER_SIZE];

	ssize_t bytes = read(conn.fd, buf, sizeof(buf));
	if (bytes <= 0) {
		_closeConnection(conn.fd);
		return;
	}

	conn.in_buffer.append(buf, bytes);

	// Step 1: locate end of headers.
	// searchPos tracks where we left off so we never re-scan old bytes.
	if (conn.headerEndCache == 0) {
		// Overlap by 3 so \r\n\r\n split across two reads is never missed.
		size_t from = conn.searchPos > 3 ? conn.searchPos - 3 : 0;
		size_t pos  = conn.in_buffer.find("\r\n\r\n", from);
		if (pos == std::string::npos) {
			conn.searchPos = conn.in_buffer.size();
			return;
		}
		conn.headerEndCache = pos + 4;
		conn.searchPos      = conn.headerEndCache;  // body starts here
		_parseBodyMeta(conn);
		if (conn.contentLength > 0)
			conn.in_buffer.reserve(conn.headerEndCache + static_cast<size_t>(conn.contentLength));
	}

	// Step 2: check body completeness, never re-scanning old bytes.
	if (conn.contentLength == 0) {
		// No body — ready immediately.
	} else if (conn.contentLength > 0) {
		// Content-Length: O(1) size check.
		if (conn.in_buffer.size() < conn.headerEndCache + static_cast<size_t>(conn.contentLength))
			return;
	} else {
		// Chunked: resume from searchPos (overlap 4 bytes for boundary safety).
		size_t from  = conn.searchPos > 4 ? conn.searchPos - 4 : conn.headerEndCache;
		size_t found = conn.in_buffer.find("0\r\n\r\n", from);
		if (found == std::string::npos) {
			conn.searchPos = conn.in_buffer.size();
			return;
		}
	}

	_processRequest(conn);
}

void EventLoop::_prepareWrite(Connection& conn) {
	conn.in_buffer.clear();
	conn.bytes_sent = 0;
	conn.state = WRITING;
	epoll_event ev;
	ev.events = EPOLLOUT;
	ev.data.fd = conn.fd;
	epoll_ctl(_epollFd, EPOLL_CTL_MOD, conn.fd, &ev);
}

void EventLoop::_logAccess(const Connection& conn, const HttpRequest& req, int status, size_t bodySize) {
	std::ostringstream oss;
	oss << conn.clientIp << " - - \""
	    << req.method << " " << req.path << " " << req.version
	    << "\" " << status << " " << bodySize;
	_logger->info(oss.str());
}

void EventLoop::_logResponse(const Connection& conn, const HttpRequest& req) {
	int status = 0;
	size_t bodySize = 0;
	size_t sp = conn.out_buffer.find(' ');
	if (sp != std::string::npos)
		status = atoi(conn.out_buffer.c_str() + sp + 1);
	size_t bodyStart = conn.out_buffer.find("\r\n\r\n");
	if (bodyStart != std::string::npos)
		bodySize = conn.out_buffer.size() - bodyStart - 4;
	_logAccess(conn, req, status, bodySize);
}

void EventLoop::_rejectOversizedBody(Connection& conn) {
	DIContainer& di = DIContainer::getInstance();
	IResponseManager& rm = di.resolve<IResponseManager>(DI_RESPONSE_MANAGER);
	conn.out_buffer = rm.buildError(413, *conn.server);
	_prepareWrite(conn);
}

void EventLoop::_processRequest(Connection& conn) {
	DIContainer& di = DIContainer::getInstance();

	IRequestParser& parser = di.resolve<IRequestParser>(DI_REQUEST_PARSER);
	parser.feed(conn.in_buffer);

	HttpRequest req;
	try {
		req = parser.parse();
	} catch (const RequestParserException&) {
		IResponseManager& rm = di.resolve<IResponseManager>(DI_RESPONSE_MANAGER);
		conn.out_buffer = rm.buildError(400, *conn.server);
		_prepareWrite(conn);
		return;
	}

	std::string pathOnly, queryString;
	splitPathAndQuery(req.path, pathOnly, queryString);

	IRouter& router = di.resolve<IRouter>(DI_ROUTER);
	const Location* location = router.route(*conn.server, pathOnly);

	size_t maxBody = location && location->clientMaxBodySize > 0
	    ? location->clientMaxBodySize
	    : conn.server->clientMaxBodySize;
	if (maxBody > 0 && req.body.size() > maxBody) {
		_logAccess(conn, req, 413, std::string("413 Content Too Large").size());
		_rejectOversizedBody(conn);
		return;
	}

	if (_tryDispatchCgi(conn, req, pathOnly, queryString, location))
		return;

	IResponseManager& rm = di.resolve<IResponseManager>(DI_RESPONSE_MANAGER);
	conn.out_buffer = rm.build(req, *conn.server, location);

	_logResponse(conn, req);
	_prepareWrite(conn);
}

void EventLoop::_handleWrite(Connection& conn) {
	conn.lastActivity = time(NULL);
	const char* data = conn.out_buffer.c_str() + conn.bytes_sent;
	size_t remaining = conn.out_buffer.size() - conn.bytes_sent;

	ssize_t sent = write(conn.fd, data, remaining);
	if (sent <= 0) {
		_closeConnection(conn.fd);
		return;
	}

	conn.bytes_sent += sent;
	if (conn.bytes_sent < conn.out_buffer.size())
		return;

	_resetToReading(conn);
}

void EventLoop::_resetToReading(Connection& conn) {
	conn.out_buffer.clear();
	conn.bytes_sent = 0;
	conn.state = READING;
	conn.headerEndCache = 0;
	conn.contentLength  = -2;
	conn.searchPos      = 0;

	epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = conn.fd;
	epoll_ctl(_epollFd, EPOLL_CTL_MOD, conn.fd, &ev);
}

bool EventLoop::_tryDispatchCgi(
	Connection& conn, HttpRequest& req,
	const std::string& pathOnly, const std::string& queryString,
	const Location* location
) {
	std::string ext = getFileExtension(pathOnly);
	if (ext.empty() || !isCgiExtension(ext, location))
		return false;

	std::string root     = (location && !location->root.empty())
	                       ? location->root : conn.server->root;
	std::string filePath = root + pathOnly;
	_startCgi(conn, req, location, filePath, queryString);
	return true;
}

void EventLoop::_initCgiContext(
	Connection& conn, HttpRequest& req,
	const CgiHandler::CgiProcess& process
) {
	conn.cgi.pid         = process.pid;
	conn.cgi.stdinFd     = process.stdinFd;
	conn.cgi.stdoutFd    = process.stdoutFd;
	conn.cgi.inputBuf.swap(req.body);  // steal body — no 100 MB copy
	conn.cgi.inputOffset = 0;
	conn.cgi.outputBuf.clear();
	conn.cgi.startTime   = time(NULL);
	conn.cgi.req         = req;
}

void EventLoop::_registerCgiPipes(
	Connection& conn, const CgiHandler::CgiProcess& process
) {
	epoll_event ev;
	ev.events  = EPOLLIN;
	ev.data.fd = process.stdoutFd;
	epoll_ctl(_epollFd, EPOLL_CTL_ADD, process.stdoutFd, &ev);
	_cgiToConn[process.stdoutFd] = conn.fd;

	if (!conn.cgi.inputBuf.empty()) {
		ev.events  = EPOLLOUT;
		ev.data.fd = process.stdinFd;
		epoll_ctl(_epollFd, EPOLL_CTL_ADD, process.stdinFd, &ev);
		_cgiToConn[process.stdinFd] = conn.fd;
		conn.state = CGI_WRITING;
	} else {
		close(process.stdinFd);
		conn.cgi.stdinFd = -1;
		conn.state = CGI_READING;
	}
}

void EventLoop::_startCgi(
	Connection& conn, HttpRequest& req,
	const Location* loc, const std::string& filePath,
	const std::string& queryString
) {
	CgiHandler::CgiProcess process;
	try {
		CgiHandler::spawn(req, *conn.server, loc, filePath, queryString, process);
	} catch (const CgiException& e) {
		_logger->error(std::string("CGI spawn failed: ") + e.what());
		DIContainer& di = DIContainer::getInstance();
		IResponseManager& rm = di.resolve<IResponseManager>(DI_RESPONSE_MANAGER);
		conn.out_buffer = rm.buildError(500, *conn.server);
		_prepareWrite(conn);
		return;
	}
	_initCgiContext(conn, req, process);
	// Body is now in cgi.inputBuf — release in_buffer immediately (up to 100 MB).
	{ std::string tmp; tmp.swap(conn.in_buffer); }
	_registerCgiPipes(conn, process);
}

void EventLoop::_handleCgi(int fd, uint32_t events) {
	std::map<int, int>::iterator cgiIt = _cgiToConn.find(fd);
	if (cgiIt == _cgiToConn.end())
		return;
	std::map<int, Connection>::iterator connIt = _connections.find(cgiIt->second);
	if (connIt == _connections.end())
		return;
	if (events & EPOLLOUT)
		_handleCgiWrite(connIt->second);
	if (events & (EPOLLIN | EPOLLHUP))
		_handleCgiRead(connIt->second);
}

void EventLoop::_handleCgiWrite(Connection& conn) {
	conn.lastActivity = time(NULL);

	const char* data      = conn.cgi.inputBuf.c_str() + conn.cgi.inputOffset;
	size_t      remaining = conn.cgi.inputBuf.size() - conn.cgi.inputOffset;

	if (remaining > 0) {
		ssize_t written = write(conn.cgi.stdinFd, data, remaining);
		if (written <= 0) {
			_abortCgi(conn, 500);
			return;
		}
		conn.cgi.inputOffset += static_cast<size_t>(written);
	}

	if (conn.cgi.inputOffset >= conn.cgi.inputBuf.size()) {
		epoll_ctl(_epollFd, EPOLL_CTL_DEL, conn.cgi.stdinFd, NULL);
		_cgiToConn.erase(conn.cgi.stdinFd);
		close(conn.cgi.stdinFd);
		conn.cgi.stdinFd = -1;
		// Release the body buffer now that CGI has consumed it (up to 100 MB).
		{ std::string tmp; tmp.swap(conn.cgi.inputBuf); }
		conn.cgi.inputOffset = 0;
		conn.state = CGI_READING;
	}
}

void EventLoop::_handleCgiRead(Connection& conn) {
	conn.lastActivity = time(NULL);

	char    buf[BUFFER_SIZE];
	ssize_t bytes = read(conn.cgi.stdoutFd, buf, sizeof(buf));

	if (bytes > 0) {
		conn.cgi.outputBuf.append(buf, static_cast<size_t>(bytes));
		return;
	}

	std::string cgiOutput = conn.cgi.outputBuf;
	HttpRequest savedReq  = conn.cgi.req;
	_cleanupCgi(conn);

	DIContainer& di = DIContainer::getInstance();
	IResponseManager& rm = di.resolve<IResponseManager>(DI_RESPONSE_MANAGER);
	conn.out_buffer = rm.buildFromCgiOutput(cgiOutput, *conn.server);

	_logResponse(conn, savedReq);
	_prepareWrite(conn);
}

void EventLoop::_cleanupCgi(Connection& conn) {
	if (conn.cgi.stdinFd >= 0) {
		epoll_ctl(_epollFd, EPOLL_CTL_DEL, conn.cgi.stdinFd, NULL);
		_cgiToConn.erase(conn.cgi.stdinFd);
		close(conn.cgi.stdinFd);
		conn.cgi.stdinFd = -1;
	}
	if (conn.cgi.stdoutFd >= 0) {
		epoll_ctl(_epollFd, EPOLL_CTL_DEL, conn.cgi.stdoutFd, NULL);
		_cgiToConn.erase(conn.cgi.stdoutFd);
		close(conn.cgi.stdoutFd);
		conn.cgi.stdoutFd = -1;
	}
	if (conn.cgi.pid > 0) {
		int status;
		waitpid(conn.cgi.pid, &status, WNOHANG);
		conn.cgi.pid = -1;
	}
}

void EventLoop::_abortCgi(Connection& conn, int statusCode) {
	if (conn.cgi.pid > 0) {
		kill(conn.cgi.pid, SIGKILL);
		int status;
		waitpid(conn.cgi.pid, &status, WNOHANG);
		conn.cgi.pid = -1;
	}
	_cleanupCgi(conn);
	conn.cgi.outputBuf.clear();
	conn.cgi.inputBuf.clear();

	DIContainer& di = DIContainer::getInstance();
	IResponseManager& rm = di.resolve<IResponseManager>(DI_RESPONSE_MANAGER);
	conn.out_buffer = rm.buildError(statusCode, *conn.server);
	_prepareWrite(conn);
}

void EventLoop::run(std::map<int, Server*>& fdToServer) {
	_fdToServer = &fdToServer;
	_logger     = &DIContainer::getInstance().resolve<ILogger>(DI_LOGGER);

	struct epoll_event events[MAX_EVENTS];

	while (_running) {
		try {
			int n = epoll_wait(_epollFd, events, MAX_EVENTS, EPOLL_TIMEOUT_MS);
			if (n == -1) {
				if (errno == EINTR)
					continue;
				throw EventLoopException("epoll_wait() failed");
			}

			for (int i = 0; i < n; ++i) {
				int fd = events[i].data.fd;

				if (fdToServer.find(fd) != fdToServer.end()) {
					_handleAcceptConnection(fd);
					continue;
				}

				std::map<int, int>::iterator cgiIt = _cgiToConn.find(fd);
				if (cgiIt != _cgiToConn.end()) {
					_handleCgi(fd, events[i].events);
					continue;
				}

				std::map<int, Connection>::iterator connection = _connections.find(fd);
				if (connection == _connections.end())
					continue;

				if (events[i].events & (EPOLLHUP | EPOLLERR))
					_closeConnection(fd);
				else if (events[i].events & EPOLLIN)
					_handleRead(connection->second);
				else if (events[i].events & EPOLLOUT)
					_handleWrite(connection->second);
			}

			_sweepIdleConnections();
		} catch (EventLoopException& e) {
			_logger->error(e.what());
		} catch (RequestParserException& e) {
			_logger->error(std::string("request parse error: ") + e.what());
		} catch (...) {
			_logger->error("unexpected error in event loop");
		}
	}
	_cleanup();
}

void EventLoop::_cleanup() {
	std::vector<int> fds;
	for (std::map<int, Connection>::iterator it = _connections.begin();
	     it != _connections.end(); ++it)
		fds.push_back(it->first);
	for (size_t i = 0; i < fds.size(); ++i)
		_closeConnection(fds[i]);

	if (_epollFd >= 0) {
		close(_epollFd);
		_epollFd = -1;
	}
}

void EventLoop::_sweepIdleConnections() {
	time_t           now     = time(NULL);
	std::vector<int> toClose;
	std::vector<int> toAbort;

	for (std::map<int, Connection>::iterator it = _connections.begin();
	     it != _connections.end(); ++it)
	{
		Connection& conn = it->second;

		if ((conn.state == CGI_WRITING || conn.state == CGI_READING) &&
		    now - conn.cgi.startTime > CGI_TIMEOUT_SECS)
		{
			toAbort.push_back(it->first);
			continue;
		}

		if (now - conn.lastActivity > IDLE_TIMEOUT_SECS)
			toClose.push_back(it->first);
	}

	for (size_t i = 0; i < toAbort.size(); ++i) {
		std::map<int, Connection>::iterator it = _connections.find(toAbort[i]);
		if (it != _connections.end()) {
			_logger->debug("CGI timeout: aborting fd=" + _itoa(toAbort[i]));
			_abortCgi(it->second, 504);
		}
	}

	for (size_t i = 0; i < toClose.size(); ++i) {
		_logger->debug("idle timeout: closing fd=" + _itoa(toClose[i]));
		_closeConnection(toClose[i]);
	}
}
