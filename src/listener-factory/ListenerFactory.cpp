#include "ListenerFactory.hpp"

ListenerFactory::ListenerFactory() {}
ListenerFactory::ListenerFactory(const ListenerFactory&) {}
ListenerFactory& ListenerFactory::operator=(const ListenerFactory&) { return *this; }
ListenerFactory::~ListenerFactory() {}

IListener* ListenerFactory::create(const std::string& host, int port) {
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1)
		throw ListenerException("socket() failed");
	try {
		ListenerUtils::setSocketOptions(fd);
		ListenerUtils::setNonBlocking(fd);
		ListenerUtils::bind(fd, host, port);
		ListenerUtils::listen(fd);
	} catch (...) {
		::close(fd);
		throw;
	}
	return new Listener(fd);
}
