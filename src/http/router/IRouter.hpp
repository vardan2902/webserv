#pragma once

#include <string>

#include "../../config.hpp"

class IRouter {
public:
	virtual ~IRouter() {}

	virtual const Location* route(const Server& server, const std::string& path) const = 0;
};
