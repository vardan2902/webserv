#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

#include "IListenerFactory.hpp"

#include "../listener/Listener.hpp"
#include "../listener/ListenerException.hpp"

class ListenerFactory : public IListenerFactory {
private:
	static const int BACKLOG = 128;

	void _setSocketOptions(int fd);
	void _setNonBlocking(int fd);
	void _bind(int fd, int port);
	void _listen(int fd);

public:
	ListenerFactory();
	ListenerFactory(const ListenerFactory& other);
	ListenerFactory& operator=(const ListenerFactory& other);
	~ListenerFactory();

	IListener* create(int port);
};
