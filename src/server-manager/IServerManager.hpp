#pragma once

class IServerManager {
public:
	virtual ~IServerManager() {}

	virtual void initializeListeningSockets() = 0;
	virtual void registerWithEventLoop() = 0;
};
