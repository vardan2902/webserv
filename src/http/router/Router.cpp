#include "Router.hpp"

Router::Router() {}
Router::Router(const Router&) {}
Router& Router::operator=(const Router&) { return *this; }
Router::~Router() {}

const Location* Router::route(const Server& server, const std::string& path) const {
	const Location* best = NULL;
	size_t bestLen = 0;

	for (size_t i = 0; i < server.locations.size(); ++i) {
		const Location& loc = server.locations[i];
		size_t locLen = loc.path.size();

		if (path.find(loc.path) != 0) continue;

		bool catchAll  = (locLen == 1 && loc.path[0] == '/');
		bool exactMatch = (path.size() == locLen);
		bool dirMatch   = (path.size() > locLen && path[locLen] == '/');

		if ((catchAll || exactMatch || dirMatch) && locLen > bestLen) {
			best    = &loc;
			bestLen = locLen;
		}
	}

	return best;
}
