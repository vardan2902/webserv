#include <unistd.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <signal.h>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <cctype>

#include "CgiHandler.hpp"

static std::string toUpperUnderscore(const std::string& s) {
	std::string result;
	for (size_t i = 0; i < s.size(); ++i) {
		char c = s[i];
		if (c == '-')
			result += '_';
		else
			result += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
	}
	return result;
}

static std::string dirOf(const std::string& path) {
	size_t slash = path.rfind('/');
	if (slash == std::string::npos || slash == 0)
		return "/";
	return path.substr(0, slash);
}

CgiHandler::CgiHandler(const HttpRequest& req, const std::string& scriptPath,
                       const std::string& interpreter, const Server& server)
	: _req(req), _scriptPath(scriptPath), _interpreter(interpreter), _server(server)
{}

CgiHandler::CgiHandler(const CgiHandler& other)
	: _req(other._req), _scriptPath(other._scriptPath),
	  _interpreter(other._interpreter), _server(other._server)
{}

CgiHandler& CgiHandler::operator=(const CgiHandler& other) {
	if (this != &other) {
		_scriptPath   = other._scriptPath;
		_interpreter  = other._interpreter;
	}
	return *this;
}

CgiHandler::~CgiHandler() {}

std::vector<std::string> CgiHandler::_buildEnv(const std::string& queryString) const {
	std::vector<std::string> env;

	std::ostringstream portStr;
	portStr << _server.port;

	std::ostringstream bodyLen;
	bodyLen << _req.body.size();

	// Mandatory CGI/1.1 meta-variables
	env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env.push_back("SERVER_PROTOCOL=" + _req.version);
	env.push_back("SERVER_SOFTWARE=webserv/1.0");
	env.push_back("SERVER_NAME=localhost");
	env.push_back("SERVER_PORT=" + portStr.str());
	env.push_back("REQUEST_METHOD=" + _req.method);
	env.push_back("QUERY_STRING=" + queryString);
	env.push_back("SCRIPT_FILENAME=" + _scriptPath);
	env.push_back("SCRIPT_NAME=" + _req.path);
	env.push_back("PATH_INFO=");
	env.push_back("PATH_TRANSLATED=");
	env.push_back("REMOTE_ADDR=127.0.0.1");
	env.push_back("REMOTE_HOST=localhost");
	env.push_back("REDIRECT_STATUS=200"); // required by php-cgi

	// Content headers
	std::map<std::string, std::string>::const_iterator it;

	it = _req.headers.find("Content-Type");
	if (it != _req.headers.end())
		env.push_back("CONTENT_TYPE=" + it->second);
	else
		env.push_back("CONTENT_TYPE=");

	it = _req.headers.find("Content-Length");
	if (it != _req.headers.end())
		env.push_back("CONTENT_LENGTH=" + it->second);
	else
		env.push_back("CONTENT_LENGTH=" + bodyLen.str());

	// HTTP_* for all request headers
	for (it = _req.headers.begin(); it != _req.headers.end(); ++it) {
		const std::string& name = it->first;
		if (name == "Content-Type" || name == "Content-Length")
			continue;
		env.push_back("HTTP_" + toUpperUnderscore(name) + "=" + it->second);
	}

	return env;
}

HttpResponse CgiHandler::_parseOutput(const std::string& raw) const {
	HttpResponse response;
	response.statusCode = 200;

	// Find header/body separator (\r\n\r\n or \n\n)
	size_t sep = raw.find("\r\n\r\n");
	std::string headerSection;
	std::string body;

	if (sep != std::string::npos) {
		headerSection = raw.substr(0, sep);
		body          = raw.substr(sep + 4);
	} else {
		sep = raw.find("\n\n");
		if (sep != std::string::npos) {
			headerSection = raw.substr(0, sep);
			body          = raw.substr(sep + 2);
		} else {
			// No header separator found — treat everything as body
			response.headers["Content-Type"] = "text/html";
			response.body = raw;
			return response;
		}
	}

	// Parse CGI response headers
	std::istringstream ss(headerSection);
	std::string line;
	while (std::getline(ss, line)) {
		// Strip trailing \r
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
		if (line.empty())
			continue;

		size_t colon = line.find(": ");
		if (colon == std::string::npos)
			continue;

		std::string hkey   = line.substr(0, colon);
		std::string hvalue = line.substr(colon + 2);

		if (hkey == "Status") {
			std::istringstream statusStream(hvalue);
			statusStream >> response.statusCode;
		} else {
			response.headers[hkey] = hvalue;
		}
	}

	// Default Content-Type if not set by CGI
	if (response.headers.find("Content-Type") == response.headers.end())
		response.headers["Content-Type"] = "text/html";

	response.body = body;
	return response;
}

std::string CgiHandler::_readWithTimeout(int fd, pid_t pid) const {
	std::string output;
	char        buf[4096];
	time_t      deadline = time(NULL) + CGI_TIMEOUT;

	while (true) {
		time_t now = time(NULL);
		if (now >= deadline) {
			kill(pid, SIGKILL);
			waitpid(pid, NULL, 0);
			throw CgiException("CGI script timed out");
		}

		struct timeval tv;
		tv.tv_sec  = deadline - now;
		tv.tv_usec = 0;

		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);

		int ret = select(fd + 1, &rfds, NULL, NULL, &tv);
		if (ret == 0) {
			kill(pid, SIGKILL);
			waitpid(pid, NULL, 0);
			throw CgiException("CGI script timed out");
		}
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			break;
		}

		ssize_t n = read(fd, buf, sizeof(buf));
		if (n <= 0)
			break;
		output.append(buf, static_cast<size_t>(n));
	}

	return output;
}

HttpResponse CgiHandler::execute() const {
	// Build environment
	std::string queryString;
	std::string pathNoQuery = _req.path;
	size_t qpos = _req.path.find('?');
	if (qpos != std::string::npos) {
		pathNoQuery  = _req.path.substr(0, qpos);
		queryString  = _req.path.substr(qpos + 1);
	}

	std::vector<std::string> envVec = _buildEnv(queryString);

	// Build null-terminated arrays for execve
	std::vector<char*> envp;
	for (size_t i = 0; i < envVec.size(); ++i)
		envp.push_back(const_cast<char*>(envVec[i].c_str()));
	envp.push_back(NULL);

	std::vector<char*> argv;
	argv.push_back(const_cast<char*>(_interpreter.c_str()));
	argv.push_back(const_cast<char*>(_scriptPath.c_str()));
	argv.push_back(NULL);

	// stdin: parent writes body → child reads
	// stdout: child writes output → parent reads
	int stdinPipe[2];
	int stdoutPipe[2];

	if (pipe(stdinPipe) == -1)
		throw CgiException(std::string("pipe() stdin failed: ") + strerror(errno));
	if (pipe(stdoutPipe) == -1) {
		close(stdinPipe[0]);
		close(stdinPipe[1]);
		throw CgiException(std::string("pipe() stdout failed: ") + strerror(errno));
	}

	pid_t pid = fork();
	if (pid == -1) {
		close(stdinPipe[0]);  close(stdinPipe[1]);
		close(stdoutPipe[0]); close(stdoutPipe[1]);
		throw CgiException(std::string("fork() failed: ") + strerror(errno));
	}

	if (pid == 0) {
		// Child: redirect stdin/stdout and exec CGI
		dup2(stdinPipe[0],  STDIN_FILENO);
		dup2(stdoutPipe[1], STDOUT_FILENO);

		close(stdinPipe[0]);  close(stdinPipe[1]);
		close(stdoutPipe[0]); close(stdoutPipe[1]);

		// Run script from its own directory
		std::string workDir = dirOf(_scriptPath);
		if (chdir(workDir.c_str()) == -1)
			std::exit(1);

		// Self-destruct if CGI hangs
		alarm(CGI_TIMEOUT);

		execve(argv[0], &argv[0], &envp[0]);
		std::exit(1); // execve failed
	}

	// Parent: close unused ends
	close(stdinPipe[0]);
	close(stdoutPipe[1]);

	// Feed request body to CGI stdin
	if (!_req.body.empty()) {
		const char* data      = _req.body.c_str();
		size_t      remaining = _req.body.size();
		while (remaining > 0) {
			ssize_t written = write(stdinPipe[1], data, remaining);
			if (written <= 0)
				break;
			data      += written;
			remaining -= static_cast<size_t>(written);
		}
	}
	close(stdinPipe[1]); // signal EOF to CGI

	// Read CGI output with timeout
	std::string output;
	try {
		output = _readWithTimeout(stdoutPipe[0], pid);
	} catch (const CgiException&) {
		close(stdoutPipe[0]);
		HttpResponse err;
		err.statusCode = 504;
		err.body       = "504 Gateway Timeout";
		err.headers["Content-Type"] = "text/plain";
		return err;
	}
	close(stdoutPipe[0]);

	int status = 0;
	waitpid(pid, &status, 0);

	if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
		HttpResponse err;
		err.statusCode = 500;
		err.body       = "500 Internal Server Error";
		err.headers["Content-Type"] = "text/plain";
		return err;
	}

	return _parseOutput(output);
}
