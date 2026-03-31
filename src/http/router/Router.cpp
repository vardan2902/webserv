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
		if (path.find(loc.path) == 0 && loc.path.size() > bestLen) {
			best = &loc;
			bestLen = loc.path.size();
		}
	}

	return best;
}
