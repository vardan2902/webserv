#pragma once

#include <map>
#include <sstream>
#include <vector>
#include <cstdlib>

#include "ISessionManager.hpp"

#define SESSION_TIMEOUT_SECS 1800

class SessionManager : public ISessionManager {
private:
	std::map<std::string, Session> _sessions;

	static std::string _generateId();
	void               _sweepExpired();

public:
	SessionManager();
	SessionManager(const SessionManager&);
	SessionManager& operator=(const SessionManager&);
	~SessionManager();

	Session& getOrCreate(const std::string& id);
	Session* get(const std::string& id);
};
