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

	void bind(int fd, const std::string& host, int port) {
		char portStr[16];
		std::snprintf(portStr, sizeof(portStr), "%d", port);

		struct addrinfo hints;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family   = AF_INET;
		hints.ai_socktype = SOCK_STREAM;

		struct addrinfo* res;
		int ret = getaddrinfo(host.c_str(), portStr, &hints, &res);
		if (ret != 0)
			throw ListenerException(std::string("getaddrinfo() failed: ") + gai_strerror(ret));

		if (::bind(fd, res->ai_addr, res->ai_addrlen) == -1) {
			freeaddrinfo(res);
			throw ListenerException("bind() failed");
		}
		freeaddrinfo(res);
	}

	void listen(int fd) {
		if (::listen(fd, BACKLOG) == -1)
			throw ListenerException("listen() failed");
	}

}