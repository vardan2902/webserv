#include <sstream>
#include "EventLoop.hpp"

int EventLoop::_epollFd = -1;

void EventLoop::initPoll() {
	_epollFd = epoll_create(1);
	if (_epollFd == -1)
		throw EventLoopException("epoll_create() failed");
}

void EventLoop::registerListener(const std::pair<const int, IListener *>& socket) {
	int fd = socket.first;
	IListener* listener = socket.second;
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.ptr = listener;
	if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, fd, &ev) == -1)
		throw EventLoopException("epoll_ctl() failed");
}

void EventLoop::registerListeners(std::map<int, IListener *>& fdToListener) {
	for (std::map<int, IListener*>::const_iterator it = fdToListener.begin(); it != fdToListener.end(); ++it)
		EventLoop::registerListener(*it);
}

std::string EventLoop::_readRequest(int clientFd) {
	std::string raw;
	char        buf[4096];

	// Read until we have the full header section
	while (raw.find("\r\n\r\n") == std::string::npos) {
		ssize_t n = read(clientFd, buf, sizeof(buf));
		if (n <= 0)
			break;
		raw.append(buf, static_cast<size_t>(n));
	}

	// Determine expected body length from Content-Length header
	size_t headerEnd = raw.find("\r\n\r\n");
	if (headerEnd == std::string::npos)
		return raw;

	size_t bodyStart = headerEnd + 4;
	size_t contentLength = 0;

	const std::string clKey = "Content-Length: ";
	size_t clPos = raw.find(clKey);
	if (clPos != std::string::npos && clPos < headerEnd) {
		size_t clEnd = raw.find("\r\n", clPos);
		std::istringstream ss(raw.substr(clPos + clKey.size(), clEnd - clPos - clKey.size()));
		ss >> contentLength;
	}

	// Keep reading until the full body arrives
	while (raw.size() < bodyStart + contentLength) {
		ssize_t n = read(clientFd, buf, sizeof(buf));
		if (n <= 0)
			break;
		raw.append(buf, static_cast<size_t>(n));
	}

	return raw;
}

void EventLoop::_dispatch(int clientFd, const std::string& raw, Server& server) {
	RequestParser parser;
	parser.feed(raw);
	HttpRequest req = parser.parse();

	Router router;
	const Location* loc = router.route(server, req.path);

	ResponseManager responseManager;
	responseManager.respond(clientFd, req, server, loc);
}

void EventLoop::_handleEvent(const epoll_event& ev, std::map<int, Server*>& fdToServer) {
	IListener* listener = reinterpret_cast<IListener*>(ev.data.ptr);
	int serverFd = listener->fd();
	int clientFd = listener->accept();
	Server* server = fdToServer[serverFd];

	std::string raw = _readRequest(clientFd);
	_dispatch(clientFd, raw, *server);
	close(clientFd);
}

void EventLoop::run(std::map<int, Server*>& fdToServer) {
	while (true) {
		try {
			struct epoll_event events[1024];
			int n = epoll_wait(_epollFd, events, 1024, -1);

			if (n == -1)
				throw EventLoopException("epoll_wait() failed");

			for (int i = 0; i < n; ++i)
				EventLoop::_handleEvent(events[i], fdToServer);
		} catch (EventLoopException& e) {
			std::cout << e.what() << std::endl;
		} catch (RequestParserException& e) {
			std::cout << e.what() << std::endl;
		} catch (...) {
			std::cout << "Uncaught Error" << std::endl;
		}
	}
}
