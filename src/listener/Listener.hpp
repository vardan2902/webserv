#pragma once

#include "IListener.hpp"
#include "ListenerException.hpp"

#include <unistd.h>
#include <sys/socket.h>

class Listener : public IListener {
private:
	int _fd;

	void _guard() const;

public:
	Listener(int fd);
	Listener(const Listener& other);
	Listener& operator=(const Listener& other);
	~Listener();

	int fd();
	int accept();
	int close();
};
