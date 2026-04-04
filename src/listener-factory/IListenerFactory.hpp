#pragma once

#include <string>
#include "../listener/IListener.hpp"

class IListenerFactory {
public:
	virtual ~IListenerFactory() {}

	virtual IListener* create(const std::string& host, int port) = 0;
};
