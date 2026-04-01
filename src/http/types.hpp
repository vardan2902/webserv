#pragma once

#include <string>
#include <map>

struct HttpRequest {
	std::string method;
	std::string path;
	std::string version;
	std::map<std::string, std::string> headers;
	std::string body;
};

struct HttpResponse {
	int         statusCode;
	std::string body;
};
