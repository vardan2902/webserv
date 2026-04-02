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

HttpResponse ResponseManager::_collect(const HttpRequest& req, const Server& server, const Location* location) const {
	std::string root  = location ? location->root  : server.root;
	std::string index = location ? location->index : "index.html";

	std::string filePath = root + req.path;
	if (filePath[filePath.size() - 1] == '/')
		filePath += index;

	std::ifstream file(filePath.c_str());

	HttpResponse response;
	if (!file.is_open()) {
		response.statusCode = 404;
		response.body = "404 Not Found";
		return response;
	}

	std::ostringstream ss;
	ss << file.rdbuf();
	response.statusCode = 200;
	response.body = ss.str();
	return response;
}

std::string ResponseManager::build_raw(const HttpResponse& response) const {
	std::ostringstream oss;
	oss << "HTTP/1.1 " << response.statusCode << " " << _statusMessage(response.statusCode) << "\r\n"
	    << "Content-Length: " << response.body.size() << "\r\n"
	    << "Content-Type: text/html\r\n"
	    << "\r\n"
	    << response.body;
	return oss.str();
}

std::string ResponseManager::build(const HttpRequest& req, const Server& server, const Location* location) const {
	HttpResponse response = _collect(req, server, location);
	return build_raw(response);
}
