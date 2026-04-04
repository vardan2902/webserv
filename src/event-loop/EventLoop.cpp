#include "EventLoop.hpp"

#include "../listener-factory/utils/ListenerUtils.hpp"

std::string EventLoop::_itoa(int n) {
	std::ostringstream oss;
	oss << n;
	return oss.str();
}

int                       EventLoop::_epollFd    = -1;
std::map<int, Server*>*   EventLoop::_fdToServer = NULL;
std::map<int, Connection> EventLoop::_connections;
ILogger*                  EventLoop::_logger     = NULL;

void EventLoop::initPoll() {
	_epollFd = epoll_create(1);
	if (_epollFd == -1)
		throw EventLoopException("epoll_create() failed");
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

static bool _headersComplete(const std::string& buf, size_t& headerEnd) {
	size_t pos = buf.find("\r\n\r\n");
	if (pos == std::string::npos)
		return false;
	headerEnd = pos + 4;
	return true;
}

static bool _bodyComplete(const std::string& buf, size_t headerEnd) {
	std::string headers = buf.substr(0, headerEnd);
	std::string clKey = "Content-Length: ";
	size_t clPos = headers.find(clKey);
	if (clPos == std::string::npos)
		return true;

	size_t valueStart = clPos + clKey.length();
	size_t valueEnd = headers.find("\r\n", valueStart);
	if (valueEnd == std::string::npos)
		return false;

	size_t contentLength = 0;
	std::istringstream iss(headers.substr(valueStart, valueEnd - valueStart));
	iss >> contentLength;
	return buf.size() >= headerEnd + contentLength;
}

void EventLoop::_closeConnection(int fd) {
	std::ostringstream msg;
	msg << "connection closed fd=" << fd;
	_logger->debug(msg.str());

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

	size_t headerEnd = 0;
	if (!_headersComplete(conn.in_buffer, headerEnd))
		return;
	if (!_bodyComplete(conn.in_buffer, headerEnd))
		return;

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

	if (conn.server->clientMaxBodySize > 0 && req.body.size() > conn.server->clientMaxBodySize) {
		_logAccess(conn, req, 413, std::string("413 Content Too Large").size());
		_rejectOversizedBody(conn);
		return;
	}

	IRouter& router = di.resolve<IRouter>(DI_ROUTER);
	const Location* location = router.route(*conn.server, req.path);

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
	if (sent < 0) {
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

	epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = conn.fd;
	epoll_ctl(_epollFd, EPOLL_CTL_MOD, conn.fd, &ev);
}

void EventLoop::run(std::map<int, Server*>& fdToServer) {
	_fdToServer = &fdToServer;
	_logger     = &DIContainer::getInstance().resolve<ILogger>(DI_LOGGER);

	struct epoll_event events[MAX_EVENTS];

	while (true) {
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

				std::map<int, Connection>::iterator connection = _connections.find(fd);
				if (connection == _connections.end())
					continue;

				if (events[i].events & EPOLLIN)
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
}

void EventLoop::_sweepIdleConnections() {
	time_t now = time(NULL);
	std::vector<int> toClose;

	for (std::map<int, Connection>::iterator it = _connections.begin();
	     it != _connections.end(); ++it)
	{
		if (now - it->second.lastActivity > IDLE_TIMEOUT_SECS)
			toClose.push_back(it->first);
	}

	for (size_t i = 0; i < toClose.size(); ++i) {
		_logger->debug("idle timeout: closing fd=" + _itoa(toClose[i]));
		_closeConnection(toClose[i]);
	}
}
