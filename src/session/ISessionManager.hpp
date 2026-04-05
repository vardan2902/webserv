#pragma once

#include <string>
#include "Session.hpp"

class ISessionManager {
public:
	virtual ~ISessionManager() {}

	virtual Session& getOrCreate(const std::string& id) = 0;
	virtual Session* get(const std::string& id)         = 0;
};
