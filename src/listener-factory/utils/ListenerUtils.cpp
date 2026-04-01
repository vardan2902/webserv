#include "ListenerUtils.hpp"

namespace ListenerUtils {
	void setNonBlocking(int fd) {
		if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1 || fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
			throw ListenerException("fcntl(F_SETFL) failed");
	}

	void setSocketOptions(int fd) {
		int opt = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
			throw ListenerException("setsockopt(SO_REUSEADDR) failed");
#ifdef SO_REUSEPORT
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) == -1)
			throw ListenerException("setsockopt(SO_REUSEPORT) failed");
#endif
	}

	void bind(int fd, int port) {
		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons(static_cast<uint16_t>(port));
		if (bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1)
			throw ListenerException("bind() failed");
	}

	void listen(int fd) {
		if (::listen(fd, BACKLOG) == -1)
			throw ListenerException("listen() failed");
	}

}