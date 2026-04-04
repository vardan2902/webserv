#pragma once

#include <sys/fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <string>

#include "../../listener/ListenerException.hpp"

#define BACKLOG 128

namespace ListenerUtils {
	void setNonBlocking(int fd);
	void setSocketOptions(int fd);
	void bind(int fd, const std::string& host, int port);
	void listen(int fd);
}
