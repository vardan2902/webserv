#pragma once

#include <string>

#include "IResponseManager.hpp"

class ResponseManager : public IResponseManager {
public:
	ResponseManager();
	ResponseManager(const ResponseManager&);
	ResponseManager& operator=(const ResponseManager&);
	~ResponseManager();

	void respond(int clientFd, const HttpRequest& req, const Server& server, const Location* location) const;

private:
	HttpResponse  _collect(const HttpRequest& req, const Server& server, const Location* location) const;
	void          _write(int clientFd, const HttpResponse& response) const;
	std::string   _statusMessage(int statusCode) const;
};
