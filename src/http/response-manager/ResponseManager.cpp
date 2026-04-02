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
		default:  return "Unknown";
	}
}

// ─── helpers ─────────────────────────────────────────────────────────────────

static HttpResponse _makeResponse(int code, const std::string& body = "") {
	HttpResponse r;
	r.statusCode = code;
	r.body = body;
	return r;
}

static bool _isMethodAllowed(const std::string& method, const std::vector<std::string>& allowed) {
	for (size_t i = 0; i < allowed.size(); ++i)
		if (allowed[i] == method) return true;
	return false;
}

// ─── pre-checks ──────────────────────────────────────────────────────────────

HttpResponse ResponseManager::_handleRedirect(const Location* location) const {
	HttpResponse response = _makeResponse(location->returnCode);
	response.headers["Location"] = location->returnUrl;
	return response;
}

// ─── method handlers ─────────────────────────────────────────────────────────

HttpResponse ResponseManager::_handleDelete(
	const HttpRequest& req, const Location*, const std::string& filePath, const std::string&
) const {
	(void)req;
	if (access(filePath.c_str(), F_OK) != 0)
		return _makeResponse(404, "404 Not Found");
	unlink(filePath.c_str());
	return _makeResponse(204);
}

HttpResponse ResponseManager::_handlePost(
	const HttpRequest&, const Location*, const std::string&, const std::string&
) const {
	return _makeResponse(200);
}

HttpResponse ResponseManager::_handleGet(
	const HttpRequest& req, const Location* location, const std::string& filePath, const std::string& index
) const {
	if (!filePath.empty() && filePath[filePath.size() - 1] == '/')
		return _serveDirectory(req, location, filePath, index);
	return _serveFile(filePath);
}

// ─── GET sub-handlers ────────────────────────────────────────────────────────

HttpResponse ResponseManager::_serveDirectory(
	const HttpRequest& req, const Location* location, const std::string& filePath, const std::string& index
) const {
	std::ifstream indexFile((filePath + index).c_str());
	if (indexFile.is_open()) {
		std::ostringstream ss;
		ss << indexFile.rdbuf();
		return _makeResponse(200, ss.str());
	}

	if (location && location->autoindex) {
		DIR* dir = opendir(filePath.c_str());
		if (!dir)
			return _makeResponse(403, "403 Forbidden");
		std::ostringstream html;
		html << "<html><head><title>Index of " << req.path << "</title></head>"
		     << "<body><h1>Index of " << req.path << "</h1><ul>";
		struct dirent* entry;
		while ((entry = readdir(dir)) != NULL)
			html << "<li><a href=\"" << entry->d_name << "\">" << entry->d_name << "</a></li>";
		closedir(dir);
		html << "</ul></body></html>";
		return _makeResponse(200, html.str());
	}

	return _makeResponse(403, "403 Forbidden");
}

HttpResponse ResponseManager::_serveFile(const std::string& filePath) const {
	std::ifstream file(filePath.c_str());
	if (!file.is_open())
		return _makeResponse(404, "404 Not Found");
	std::ostringstream ss;
	ss << file.rdbuf();
	return _makeResponse(200, ss.str());
}

// ─── collect ─────────────────────────────────────────────────────────────────

HttpResponse ResponseManager::_collect(const HttpRequest& req, const Server& server, const Location* location) const {
	if (location && location->returnCode != 0)
		return _handleRedirect(location);

	if (location && !location->allowMethods.empty() && !_isMethodAllowed(req.method, location->allowMethods))
		return _makeResponse(405, "405 Method Not Allowed");

	std::string root     = location && !location->root.empty()  ? location->root  : server.root;
	std::string index    = location && !location->index.empty() ? location->index : "index.html";
	std::string filePath = root + req.path;

	typedef HttpResponse (ResponseManager::*MethodHandler)(
		const HttpRequest&, const Location*, const std::string&, const std::string&
	) const;
	typedef std::map<std::string, MethodHandler> MethodHandlerMap;

	static MethodHandlerMap handlers;
	if (handlers.empty()) {
		handlers["DELETE"] = &ResponseManager::_handleDelete;
		handlers["POST"]   = &ResponseManager::_handlePost;
		handlers["GET"]    = &ResponseManager::_handleGet;
	}

	MethodHandlerMap::const_iterator it = handlers.find(req.method);
	if (it == handlers.end())
		return _makeResponse(405, "405 Method Not Allowed");
	return (this->*it->second)(req, location, filePath, index);
}

// ─── build ───────────────────────────────────────────────────────────────────

std::string ResponseManager::build_raw(const HttpResponse& response) const {
	std::ostringstream oss;
	oss << "HTTP/1.1 " << response.statusCode << " " << _statusMessage(response.statusCode) << "\r\n"
	    << "Content-Length: " << response.body.size() << "\r\n"
	    << "Content-Type: text/html\r\n";
	for (std::map<std::string, std::string>::const_iterator it = response.headers.begin(); it != response.headers.end(); ++it)
		oss << it->first << ": " << it->second << "\r\n";
	oss << "\r\n" << response.body;
	return oss.str();
}

std::string ResponseManager::build(const HttpRequest& req, const Server& server, const Location* location) const {
	HttpResponse response = _collect(req, server, location);

	if (response.statusCode >= 400 && response.statusCode <= 599) {
		std::map<int, std::string>::const_iterator it = server.errorPages.find(response.statusCode);
		if (it != server.errorPages.end()) {
			std::ifstream errFile((server.root + it->second).c_str());
			if (errFile.is_open()) {
				std::ostringstream ss;
				ss << errFile.rdbuf();
				response.body = ss.str();
			}
		}
	}

	return build_raw(response);
}
