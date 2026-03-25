#pragma once
#include <string>

class ILogger {
public:
	virtual void info(const std::string&) = 0;
	virtual void error(const std::string&) = 0;
	virtual void debug(const std::string&) = 0;
};
