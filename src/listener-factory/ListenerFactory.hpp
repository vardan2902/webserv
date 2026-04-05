#pragma once

#include "IListenerFactory.hpp"
#include "utils/ListenerUtils.hpp"
#include "../listener/Listener.hpp"
#include "../listener/ListenerException.hpp"

class ListenerFactory : public IListenerFactory {
public:
	ListenerFactory();
	ListenerFactory(const ListenerFactory& other);
	ListenerFactory& operator=(const ListenerFactory& other);
	~ListenerFactory();

	IListener* create(const std::string& host, int port);
};
