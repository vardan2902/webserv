#pragma once

#include "IRouter.hpp"

class Router : public IRouter {
public:
	Router();
	Router(const Router&);
	Router& operator=(const Router&);
	~Router();

	const Location* route(const Server& server, const std::string& path) const;
};
