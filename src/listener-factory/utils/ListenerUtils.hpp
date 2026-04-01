#pragma once

#include <sys/fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

#include "../../listener/ListenerException.hpp"

#define BACKLOG 128

namespace ListenerUtils {
	void setNonBlocking(int fd);
	void setSocketOptions(int fd);
	void bind(int fd, int port);
	void listen(int fd);
}
