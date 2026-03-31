#pragma once

#include "../types.hpp"

class IRequestParser {
public:
	virtual ~IRequestParser() {}

	virtual void feed(const std::string& raw) = 0;
	virtual HttpRequest parse() = 0;
};
