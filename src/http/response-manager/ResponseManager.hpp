#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <unistd.h>

#include "IResponseManager.hpp"

class ResponseManager : public IResponseManager {
public:
	ResponseManager();
	ResponseManager(const ResponseManager&);
	ResponseManager& operator=(const ResponseManager&);
	~ResponseManager();

	std::string build(const HttpRequest& req, const Server& server, const Location* location) const;

private:
	HttpResponse  _collect(const HttpRequest& req, const Server& server, const Location* location) const;
	std::string   build_raw(const HttpResponse& response) const;
	std::string   _statusMessage(int statusCode) const;
};
