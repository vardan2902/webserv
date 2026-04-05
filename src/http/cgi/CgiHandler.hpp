#pragma once

#include <string>
#include <vector>
#include <sys/types.h>

#include "../../config.hpp"
#include "../types.hpp"

class CgiHandler {
public:
	struct CgiProcess {
		pid_t  pid;
		int    stdinFd;   // parent writes request body here
		int    stdoutFd;  // parent reads CGI output from here

		CgiProcess() : pid(-1), stdinFd(-1), stdoutFd(-1) {}
	};

	static bool         isCgi(const std::string& ext, const Location* loc);
	static std::string  getExtension(const std::string& filePath);
	static void         splitPath(const std::string& reqPath,
	                               std::string& pathOnly, std::string& query);
	static bool         start(const HttpRequest& req, const Server& server,
	                           const Location* loc, const std::string& filePath,
	                           const std::string& queryString, CgiProcess& out);
	static HttpResponse parseOutput(const std::string& raw);

private:
	static std::string              _getInterpreter(const std::string& ext,
	                                                 const Location* loc);
	static std::vector<std::string> _buildEnv(const HttpRequest& req,
	                                           const Server& server,
	                                           const std::string& filePath,
	                                           const std::string& queryString);
};
