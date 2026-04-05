#include "SessionManager.hpp"

SessionManager::SessionManager() {}
SessionManager::SessionManager(const SessionManager&) {}
SessionManager& SessionManager::operator=(const SessionManager&) { return *this; }
SessionManager::~SessionManager() {}

std::string SessionManager::_generateId() {
	static unsigned int counter = 0;
	++counter;
	std::ostringstream oss;
	oss << std::hex << static_cast<unsigned long>(time(NULL))
	    << std::hex << counter
	    << std::hex << static_cast<unsigned int>(rand());
	return oss.str();
}

void SessionManager::_sweepExpired() {
	time_t now = time(NULL);
	std::vector<std::string> toErase;

	for (std::map<std::string, Session>::iterator it = _sessions.begin();
	     it != _sessions.end(); ++it)
	{
		if (now - it->second.lastAccess > SESSION_TIMEOUT_SECS)
			toErase.push_back(it->first);
	}

	for (size_t i = 0; i < toErase.size(); ++i)
		_sessions.erase(toErase[i]);
}

Session& SessionManager::getOrCreate(const std::string& id) {
	_sweepExpired();

	if (!id.empty()) {
		std::map<std::string, Session>::iterator it = _sessions.find(id);
		if (it != _sessions.end()) {
			it->second.lastAccess = time(NULL);
			return it->second;
		}
	}

	Session s;
	s.id         = _generateId();
	s.createdAt  = time(NULL);
	s.lastAccess = s.createdAt;
	_sessions[s.id] = s;
	return _sessions[s.id];
}

Session* SessionManager::get(const std::string& id) {
	if (id.empty())
		return NULL;
	std::map<std::string, Session>::iterator it = _sessions.find(id);
	if (it == _sessions.end())
		return NULL;
	return &it->second;
}
