#include "CgiHandler.hpp"
#include "CgiException.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <sstream>
#include <cctype>
#include <sys/wait.h>

void splitPathAndQuery(const std::string& reqPath,
                        std::string& pathOnly, std::string& query) {
	size_t q = reqPath.find('?');
	if (q == std::string::npos) {
		pathOnly = reqPath;
		query    = "";
	} else {
		pathOnly = reqPath.substr(0, q);
		query    = reqPath.substr(q + 1);
	}
}

std::string getFileExtension(const std::string& filePath) {
	size_t slash = filePath.rfind('/');
	size_t dot   = filePath.rfind('.');
	if (dot == std::string::npos)
		return "";
	if (slash != std::string::npos && dot < slash)
		return "";
	return filePath.substr(dot);
}

bool isCgiExtension(const std::string& ext, const Location* loc) {
	if (!loc || ext.empty())
		return false;
	return loc->cgiExtensions.count(ext) > 0;
}

std::string CgiHandler::_findInterpreter(const std::string& ext, const Location* loc) {
	if (!loc) return "";
	std::map<std::string, std::string>::const_iterator it = loc->cgiExtensions.find(ext);
	if (it == loc->cgiExtensions.end()) return "";
	return it->second;
}

static std::string _intToStr(int n) {
	std::ostringstream oss;
	oss << n;
	return oss.str();
}

static std::string _headerToEnvKey(const std::string& header) {
	std::string key = "HTTP_";
	for (size_t i = 0; i < header.size(); ++i) {
		if (header[i] == '-')
			key += '_';
		else
			key += static_cast<char>(std::toupper(static_cast<unsigned char>(header[i])));
	}
	return key;
}

void CgiHandler::_addGatewayVars(std::vector<std::string>& env) {
	env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env.push_back("SERVER_PROTOCOL=HTTP/1.1");
	env.push_back("SERVER_SOFTWARE=webserv/1.0");
	env.push_back("REDIRECT_STATUS=200");
}

void CgiHandler::_addServerVars(
	std::vector<std::string>& env,
	const Server& server, const HttpRequest& req
) {
	env.push_back("SERVER_PORT=" + _intToStr(server.port));

	std::string serverName = server.host;
	std::map<std::string, std::string>::const_iterator hostIt = req.headers.find("Host");
	if (hostIt != req.headers.end() && !hostIt->second.empty()) {
		serverName = hostIt->second;
		size_t colon = serverName.find(':');
		if (colon != std::string::npos)
			serverName = serverName.substr(0, colon);
	}
	env.push_back("SERVER_NAME=" + serverName);
}

void CgiHandler::_addRequestVars(
	std::vector<std::string>& env, const HttpRequest& req,
	const std::string& filePath, const std::string& queryString
) {
	env.push_back("REQUEST_METHOD=" + req.method);

	std::string pathOnly, dummy;
	splitPathAndQuery(req.path, pathOnly, dummy);
	env.push_back("SCRIPT_NAME=" + pathOnly);
	env.push_back("SCRIPT_FILENAME=" + filePath);
	env.push_back("PATH_INFO=");
	env.push_back("PATH_TRANSLATED=");
	env.push_back("QUERY_STRING=" + queryString);
}

void CgiHandler::_addContentVars(
	std::vector<std::string>& env, const HttpRequest& req
) {
	std::map<std::string, std::string>::const_iterator ctIt =
		req.headers.find("Content-Type");
	env.push_back("CONTENT_TYPE=" +
		(ctIt != req.headers.end() ? ctIt->second : std::string("")));

	std::map<std::string, std::string>::const_iterator clIt =
		req.headers.find("Content-Length");
	env.push_back("CONTENT_LENGTH=" +
		(clIt != req.headers.end() ? clIt->second : _intToStr(static_cast<int>(req.body.size()))));
}

void CgiHandler::_addHttpHeaderVars(
	std::vector<std::string>& env, const HttpRequest& req
) {
	for (std::map<std::string, std::string>::const_iterator it = req.headers.begin();
	     it != req.headers.end(); ++it)
	{
		if (it->first == "Content-Type" || it->first == "Content-Length")
			continue;
		env.push_back(_headerToEnvKey(it->first) + "=" + it->second);
	}
}

std::vector<std::string> CgiHandler::_buildCgiEnv(
	const HttpRequest& req,
	const Server& server,
	const std::string& filePath,
	const std::string& queryString
) {
	std::vector<std::string> env;
	_addGatewayVars(env);
	_addServerVars(env, server, req);
	_addRequestVars(env, req, filePath, queryString);
	_addContentVars(env, req);
	_addHttpHeaderVars(env, req);
	return env;
}

bool CgiHandler::_openPipes(int stdinPipe[2], int stdoutPipe[2]) {
	if (pipe(stdinPipe) < 0)
		return false;
	if (pipe(stdoutPipe) < 0) {
		close(stdinPipe[0]);
		close(stdinPipe[1]);
		return false;
	}
	return true;
}

void CgiHandler::_execCgiProcess(
	int stdinRead, int stdoutWrite,
	const std::string& interpreter,
	const std::string& filePath,
	const std::vector<std::string>& envVec
) {
	if (dup2(stdinRead, STDIN_FILENO) < 0)   _exit(1);
	if (dup2(stdoutWrite, STDOUT_FILENO) < 0) _exit(1);
	close(stdinRead);
	close(stdoutWrite);

	size_t slash = filePath.rfind('/');
	if (slash != std::string::npos) {
		std::string dir = filePath.substr(0, slash);
		if (chdir(dir.c_str()) < 0) _exit(1);
	}

	std::vector<char*> argv;
	argv.push_back(const_cast<char*>(interpreter.c_str()));
	argv.push_back(const_cast<char*>(filePath.c_str()));
	argv.push_back(NULL);

	std::vector<char*> envp;
	for (size_t i = 0; i < envVec.size(); ++i)
		envp.push_back(const_cast<char*>(envVec[i].c_str()));
	envp.push_back(NULL);

	execve(interpreter.c_str(), &argv[0], &envp[0]);
	_exit(1);
}

void CgiHandler::spawn(
	const HttpRequest& req,
	const Server& server,
	const Location* loc,
	const std::string& filePath,
	const std::string& queryString,
	CgiProcess& out
) {
	std::string interpreter = _findInterpreter(getFileExtension(filePath), loc);
	if (interpreter.empty())
		throw CgiException("no interpreter configured for: " + filePath);

	std::vector<std::string> envVec = _buildCgiEnv(req, server, filePath, queryString);

	int stdinPipe[2];
	int stdoutPipe[2];
	if (!_openPipes(stdinPipe, stdoutPipe))
		throw CgiException("pipe() failed");

	pid_t pid = fork();
	if (pid < 0) {
		close(stdinPipe[0]);  close(stdinPipe[1]);
		close(stdoutPipe[0]); close(stdoutPipe[1]);
		throw CgiException("fork() failed");
	}

	if (pid == 0) {
		close(stdinPipe[1]);
		close(stdoutPipe[0]);
		_execCgiProcess(stdinPipe[0], stdoutPipe[1], interpreter, filePath, envVec);
	}

	close(stdinPipe[0]);
	close(stdoutPipe[1]);

	fcntl(stdinPipe[1],  F_SETFL, O_NONBLOCK);
	fcntl(stdoutPipe[0], F_SETFL, O_NONBLOCK);

	out.pid      = pid;
	out.stdinFd  = stdinPipe[1];
	out.stdoutFd = stdoutPipe[0];
}

bool CgiHandler::_findHeaderBodyBoundary(const std::string& raw,
                                          size_t& sep, std::string& delim) {
	sep = raw.find("\r\n\r\n");
	if (sep != std::string::npos) {
		delim = "\r\n\r\n";
		return true;
	}
	sep = raw.find("\n\n");
	if (sep != std::string::npos) {
		delim = "\n\n";
		return true;
	}
	return false;
}

void CgiHandler::_parseHeaderLines(const std::string& headerPart, HttpResponse& resp) {
	std::istringstream ss(headerPart);
	std::string line;
	while (std::getline(ss, line)) {
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
		if (line.empty())
			continue;

		size_t colon = line.find(": ");
		if (colon == std::string::npos)
			continue;

		std::string key = line.substr(0, colon);
		std::string val = line.substr(colon + 2);

		if (key == "Status") {
			std::istringstream iss(val);
			iss >> resp.statusCode;
		} else {
			resp.headers[key] = val;
		}
	}
}

HttpResponse CgiHandler::parseResponse(const std::string& raw) {
	HttpResponse resp;
	resp.statusCode = 200;

	size_t      sep;
	std::string delim;
	if (!_findHeaderBodyBoundary(raw, sep, delim)) {
		resp.body = raw;
		resp.headers["Content-Type"] = "text/html";
		return resp;
	}

	std::string headerPart = raw.substr(0, sep);
	resp.body              = raw.substr(sep + delim.size());

	_parseHeaderLines(headerPart, resp);

	if (resp.headers.find("Content-Type") == resp.headers.end())
		resp.headers["Content-Type"] = "text/html";

	return resp;
}
