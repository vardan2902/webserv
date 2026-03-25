#include "Listener.hpp"

void Listener::_guard() const {
	if (_fd == -1)
		throw ListenerException("socket is closed");
}

Listener::Listener(int fd) : _fd(fd) {}

Listener::Listener(const Listener& other) : _fd(dup(other._fd)) {}

Listener& Listener::operator=(const Listener& other) {
	if (this != &other) {
		::close(_fd);
		_fd = dup(other._fd);
	}
	return *this;
}

Listener::~Listener() {
	if (_fd != -1)
		::close(_fd);
}

int Listener::fd() {
	_guard();
	return _fd;
}

int Listener::accept() {
	_guard();
	return ::accept(_fd, NULL, NULL);
}

int Listener::close() {
	_guard();
	int result = ::close(_fd);
	_fd = -1;
	return result;
}
