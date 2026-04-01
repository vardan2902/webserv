#pragma once

#include "../listener/IListener.hpp"

class IListenerFactory {
public:
	virtual ~IListenerFactory() {}

	virtual IListener* create(int port) = 0;
};
