#pragma once

#include "../types.hpp"
#include "../../config.hpp"

class IResponseManager {
public:
	virtual ~IResponseManager() {}

	virtual void respond(int clientFd, const HttpRequest& req, const Server& server, const Location* location) const = 0;
};
