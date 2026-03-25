#include "ListenerFactory.hpp"

ListenerFactory::ListenerFactory() {}
ListenerFactory::ListenerFactory(const ListenerFactory&) {}
ListenerFactory& ListenerFactory::operator=(const ListenerFactory&) { return *this; }
ListenerFactory::~ListenerFactory() {}

IListener* ListenerFactory::create(int port) {
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1)
		throw ListenerException("socket() failed");
	try {
		_setSocketOptions(fd);
		_setNonBlocking(fd);
		_bind(fd, port);
		_listen(fd);
	} catch (...) {
		::close(fd);
		throw;
	}
	return new Listener(fd);
}

void ListenerFactory::_setSocketOptions(int fd) {
	int opt = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
		throw ListenerException("setsockopt(SO_REUSEADDR) failed");
#ifdef SO_REUSEPORT
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) == -1)
		throw ListenerException("setsockopt(SO_REUSEPORT) failed");
#endif
}

void ListenerFactory::_setNonBlocking(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		throw ListenerException("fcntl(F_GETFL) failed");
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
		throw ListenerException("fcntl(F_SETFL) failed");
}

void ListenerFactory::_bind(int fd, int port) {
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(static_cast<uint16_t>(port));
	if (bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1)
		throw ListenerException("bind() failed");
}

void ListenerFactory::_listen(int fd) {
	if (listen(fd, BACKLOG) == -1)
		throw ListenerException("listen() failed");
}
