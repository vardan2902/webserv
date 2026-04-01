#include <sstream>

#include "RequestParser.hpp"
#include "RequestParserException.hpp"

RequestParser::RequestParser() {}
RequestParser::RequestParser(const RequestParser&) {}
RequestParser& RequestParser::operator=(const RequestParser&) { return *this; }
RequestParser::~RequestParser() {}

void RequestParser::feed(const std::string& req) {
	_raw = req;
}

std::string RequestParser::_readLine(size_t& pos) const {
	size_t end = _raw.find("\r\n", pos);
	if (end == std::string::npos)
		throw RequestParserException("malformed request: missing CRLF");
	std::string line = _raw.substr(pos, end - pos);
	pos = end + 2;
	return line;
}

void RequestParser::_parseRequestLine(HttpRequest& req, size_t& pos) const {
	std::string line = _readLine(pos);

	size_t first = line.find(' ');
	if (first == std::string::npos)
		throw RequestParserException("malformed request line: " + line);

	size_t second = line.find(' ', first + 1);
	if (second == std::string::npos)
		throw RequestParserException("malformed request line: " + line);

	req.method  = line.substr(0, first);
	req.path    = line.substr(first + 1, second - first - 1);
	req.version = line.substr(second + 1);

	if (req.method.empty() || req.path.empty() || req.version.empty())
		throw RequestParserException("malformed request line: " + line);
}

void RequestParser::_parseHeaders(HttpRequest& req, size_t& pos) const {
	while (true) {
		std::string line = _readLine(pos);
		if (line.empty())
			break;

		size_t colon = line.find(": ");
		if (colon == std::string::npos)
			throw RequestParserException("malformed header: " + line);

		req.headers[line.substr(0, colon)] = line.substr(colon + 2);
	}
}

void RequestParser::_parseBody(HttpRequest& req, size_t pos) const {
	std::map<std::string, std::string>::const_iterator it = req.headers.find("Content-Length");
	if (it == req.headers.end())
		return;

	size_t length;
	std::istringstream(it->second) >> length;

	if (pos + length > _raw.size())
		throw RequestParserException("body shorter than Content-Length");

	req.body = _raw.substr(pos, length);
}

HttpRequest RequestParser::parse() {
	HttpRequest parsedRequest;

	size_t pos = 0;
	_parseRequestLine(parsedRequest, pos);
	_parseHeaders(parsedRequest, pos);
	_parseBody(parsedRequest, pos);

	return parsedRequest;
}
