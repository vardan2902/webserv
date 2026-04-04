#pragma once

#include "../types.hpp"
#include "../../config.hpp"

class IResponseManager {
public:
	virtual ~IResponseManager() {}

	virtual std::string build(const HttpRequest& req, const Server& server, const Location* location) const = 0;
	virtual std::string buildError(int code, const Server& server) const = 0;
};
