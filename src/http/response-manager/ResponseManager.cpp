#include <fstream>
#include <sstream>
#include <unistd.h>

#include "ResponseManager.hpp"

ResponseManager::ResponseManager() {}
ResponseManager::ResponseManager(const ResponseManager&) {}
ResponseManager& ResponseManager::operator=(const ResponseManager&) { return *this; }
ResponseManager::~ResponseManager() {}

std::string ResponseManager::_statusMessage(int statusCode) const {
	switch (statusCode) {
		case 200: return "OK";
		case 201: return "Created";
		case 204: return "No Content";
		case 400: return "Bad Request";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 500: return "Internal Server Error";
		case 504: return "Gateway Timeout";
		default:  return "Unknown";
	}
}

// Returns the file extension including the dot, e.g. ".php"
// Returns empty string if the last path component has no dot.
static std::string fileExtension(const std::string& path) {
	size_t lastSlash = path.rfind('/');
	size_t dot       = path.rfind('.');
	if (dot == std::string::npos)
		return "";
	if (lastSlash != std::string::npos && dot < lastSlash)
		return "";
	return path.substr(dot);
}

// Read a file into a string. Returns empty string if file cannot be opened.
static std::string readFile(const std::string& path) {
	std::ifstream f(path.c_str());
	if (!f.is_open())
		return "";
	std::ostringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

HttpResponse ResponseManager::_errorResponse(int code, const Server& server) const {
	HttpResponse response;
	response.statusCode = code;
	response.headers["Content-Type"] = "text/html";

	// Try custom error page defined in config
	std::map<int, std::string>::const_iterator it = server.errorPages.find(code);
	if (it != server.errorPages.end()) {
		std::string body = readFile(server.root + it->second);
		if (!body.empty()) {
			response.body = body;
			return response;
		}
	}

	// Built-in fallback (plain HTML, no external file needed)
	std::string msg = _statusMessage(code);
	std::ostringstream html;
	html << "<!DOCTYPE html>\n"
	     << "<html lang=\"en\"><head><meta charset=\"UTF-8\">"
	     << "<title>" << code << " " << msg << "</title>"
	     << "<style>"
	     << "body{font-family:system-ui,sans-serif;background:#f5f5f5;"
	     << "display:flex;align-items:center;justify-content:center;"
	     << "height:100vh;margin:0}"
	     << "div{text-align:center}"
	     << "h1{font-size:6rem;margin:0;color:#222}"
	     << "p{font-size:1.2rem;color:#555;margin:0.5rem 0 1.5rem}"
	     << "a{color:#0066cc}"
	     << "</style></head>\n"
	     << "<body><div>"
	     << "<h1>" << code << "</h1>"
	     << "<p>" << msg << "</p>"
	     << "<a href=\"/\">Go back home</a>"
	     << "</div></body></html>\n";
	response.body = html.str();
	return response;
}

HttpResponse ResponseManager::_collect(const HttpRequest& req, const Server& server, const Location* location) const {
	std::string root  = (location && !location->root.empty())  ? location->root  : server.root;
	std::string index = (location && !location->index.empty()) ? location->index : "index.html";

	// Strip query string to get clean path
	std::string pathNoQuery = req.path;
	size_t qpos = req.path.find('?');
	if (qpos != std::string::npos)
		pathNoQuery = req.path.substr(0, qpos);

	std::string filePath = root + pathNoQuery;
	if (!filePath.empty() && filePath[filePath.size() - 1] == '/')
		filePath += index;

	// Check if this path matches a registered CGI extension
	if (location != NULL && !location->cgiHandlers.empty()) {
		std::string ext = fileExtension(filePath);
		if (!ext.empty()) {
			std::map<std::string, std::string>::const_iterator it = location->cgiHandlers.find(ext);
			if (it != location->cgiHandlers.end()) {
				CgiHandler cgi(req, filePath, it->second, server);
				return cgi.execute();
			}
		}
	}

	// Static file serving
	std::ifstream file(filePath.c_str());
	if (!file.is_open())
		return _errorResponse(404, server);

	std::ostringstream ss;
	ss << file.rdbuf();

	HttpResponse response;
	response.statusCode = 200;
	response.body       = ss.str();
	response.headers["Content-Type"] = "text/html";
	return response;
}

void ResponseManager::_write(int clientFd, const HttpResponse& response) const {
	std::ostringstream oss;

	oss << "HTTP/1.1 " << response.statusCode << " " << _statusMessage(response.statusCode) << "\r\n";
	oss << "Content-Length: " << response.body.size() << "\r\n";

	// Write all response headers (Content-Type, etc.)
	std::map<std::string, std::string>::const_iterator it;
	for (it = response.headers.begin(); it != response.headers.end(); ++it)
		oss << it->first << ": " << it->second << "\r\n";

	// Default Content-Type if not in headers
	if (response.headers.find("Content-Type") == response.headers.end())
		oss << "Content-Type: text/html\r\n";

	oss << "\r\n";
	oss << response.body;

	std::string raw = oss.str();
	::write(clientFd, raw.c_str(), raw.size());
}

void ResponseManager::respond(int clientFd, const HttpRequest& req, const Server& server, const Location* location) const {
	HttpResponse response = _collect(req, server, location);
	_write(clientFd, response);
}
