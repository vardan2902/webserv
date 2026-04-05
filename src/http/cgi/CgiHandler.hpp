#pragma once

#include <string>
#include <vector>
#include <sys/types.h>

#include "../../config.hpp"
#include "../types.hpp"

void        splitPathAndQuery(const std::string& reqPath,
                               std::string& pathOnly, std::string& query);
std::string getFileExtension(const std::string& filePath);
bool        isCgiExtension(const std::string& ext, const Location* loc);

class CgiHandler {
public:
	struct CgiProcess {
		pid_t  pid;
		int    stdinFd;
		int    stdoutFd;

		CgiProcess() : pid(-1), stdinFd(-1), stdoutFd(-1) {}
	};

	static bool         spawn(const HttpRequest& req, const Server& server,
	                           const Location* loc, const std::string& filePath,
	                           const std::string& queryString, CgiProcess& out);
	static HttpResponse parseResponse(const std::string& raw);

private:
	static std::string              _findInterpreter(const std::string& ext,
	                                                  const Location* loc);
	static std::vector<std::string> _buildCgiEnv(const HttpRequest& req,
	                                               const Server& server,
	                                               const std::string& filePath,
	                                               const std::string& queryString);
	static bool                     _openPipes(int stdinPipe[2], int stdoutPipe[2]);
	static void                     _execCgiProcess(int stdinRead, int stdoutWrite,
	                                                 const std::string& interpreter,
	                                                 const std::string& filePath,
	                                                 const std::vector<std::string>& envVec);
	static bool                     _findHeaderBodyBoundary(const std::string& raw,
	                                                         size_t& sep,
	                                                         std::string& delim);
	static void                     _parseHeaderLines(const std::string& headerPart,
	                                                   HttpResponse& resp);
};
