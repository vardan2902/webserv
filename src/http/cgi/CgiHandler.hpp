#pragma once

#include <string>
#include <vector>
#include <map>

#include "../types.hpp"
#include "../../config.hpp"
#include "CgiException.hpp"

#define CGI_TIMEOUT 30

class CgiHandler {
public:
	CgiHandler(const HttpRequest& req, const std::string& scriptPath,
	           const std::string& interpreter, const Server& server);
	CgiHandler(const CgiHandler&);
	CgiHandler& operator=(const CgiHandler&);
	~CgiHandler();

	HttpResponse execute() const;

private:
	const HttpRequest& _req;
	std::string        _scriptPath;
	std::string        _interpreter;
	const Server&      _server;

	std::vector<std::string> _buildEnv(const std::string& queryString) const;
	HttpResponse             _parseOutput(const std::string& raw) const;
	std::string              _readWithTimeout(int fd, pid_t pid) const;
};
