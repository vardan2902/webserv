#pragma once

#include <string>
#include <map>
#include <ctime>

struct Session {
	std::string                        id;
	std::map<std::string, std::string> data;
	time_t                             createdAt;
	time_t                             lastAccess;
};
