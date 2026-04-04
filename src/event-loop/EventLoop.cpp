#include "EventLoop.hpp"

#include "../listener-factory/utils/ListenerUtils.hpp"

int EventLoop::_epollFd = -1;
std::map<int, Server*>* EventLoop::_fdToServer = NULL;
std::map<int, Connection> EventLoop::_connections;

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

void EventLoop::_handleAcceptConnection(int fd) {
	while (true) {
		try {
			int client_fd = ::accept(fd, NULL, NULL);
			if (client_fd < 0)
				return;

			ListenerUtils::setNonBlocking(client_fd);

			epoll_event ev;
			ev.events = EPOLLIN;
			ev.data.fd = client_fd;

			epoll_ctl(_epollFd, EPOLL_CTL_ADD, client_fd, &ev);

			Connection conn;
			conn.fd = client_fd;
			conn.state = READING;
			conn.bytes_sent = 0;
			conn.server = (*_fdToServer)[fd];

			_connections[client_fd] = conn;
		} catch (const EventLoopException& e) {
			std::cout << e.what() << std::endl;
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
	epoll_ctl(_epollFd, EPOLL_CTL_DEL, fd, NULL);
	close(fd);
	_connections.erase(fd);
}

void EventLoop::_handleRead(Connection& conn) {
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

void EventLoop::_rejectOversizedBody(Connection& conn) {
	ResponseManager rm;
	conn.out_buffer = rm.buildError(413, *conn.server);
	_prepareWrite(conn);
}

void EventLoop::_processRequest(Connection& conn) {
	ResponseManager rm;

	RequestParser parser;
	parser.feed(conn.in_buffer);

	HttpRequest req;
	try {
		req = parser.parse();
	} catch (const RequestParserException&) {
		conn.out_buffer = rm.buildError(400, *conn.server);
		_prepareWrite(conn);
		return;
	}

	if (conn.server->clientMaxBodySize > 0 && req.body.size() > conn.server->clientMaxBodySize) {
		_rejectOversizedBody(conn);
		return;
	}

	Router router;
	const Location* location = router.route(*conn.server, req.path);

	conn.out_buffer = rm.build(req, *conn.server, location);
	_prepareWrite(conn);
}

void EventLoop::_handleWrite(Connection& conn) {
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
	struct epoll_event events[MAX_EVENTS];

	while (true) {
		try {
			int n = epoll_wait(_epollFd, events, 1024, -1);
			if (n == -1)
				throw EventLoopException("epoll_wait() failed");

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
		} catch (EventLoopException& e) {
			std::cout << e.what() << std::endl;
		} catch (RequestParserException& e) {
			std::cout << e.what() << std::endl;
		} catch (...) {
			std::cout << "Uncaught Error" << std::endl;
		}
	}
}
