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

std::string RequestParser::_decodeChunked(size_t pos) const {
	std::string result;
	while (true) {
		std::string sizeLine = _readLine(pos);
		size_t chunkSize = 0;
		std::istringstream iss(sizeLine);
		iss >> std::hex >> chunkSize;
		if (chunkSize == 0)
			break;
		if (pos + chunkSize > _raw.size())
			throw RequestParserException("chunked body: chunk data truncated");
		result.append(_raw, pos, chunkSize);
		pos += chunkSize;
		if (pos + 2 > _raw.size() || _raw[pos] != '\r' || _raw[pos + 1] != '\n')
			throw RequestParserException("chunked body: missing CRLF after chunk data");
		pos += 2;
	}
	return result;
}

void RequestParser::_parseBody(HttpRequest& req, size_t pos) const {
	std::map<std::string, std::string>::const_iterator te = req.headers.find("Transfer-Encoding");
	if (te != req.headers.end() && te->second == "chunked") {
		req.body = _decodeChunked(pos);
		return;
	}

	std::map<std::string, std::string>::const_iterator it = req.headers.find("Content-Length");
	if (it == req.headers.end())
		return;

	size_t length;
	std::istringstream(it->second) >> length;

	if (pos + length > _raw.size())
		throw RequestParserException("body shorter than Content-Length");

	req.body = _raw.substr(pos, length);
}

void RequestParser::_parseCookies(HttpRequest& req) const {
	std::map<std::string, std::string>::const_iterator it = req.headers.find("Cookie");
	if (it == req.headers.end())
		return;

	const std::string& header = it->second;
	size_t pos = 0;
	while (pos < header.size()) {
		while (pos < header.size() && header[pos] == ' ')
			++pos;

		size_t eq = header.find('=', pos);
		if (eq == std::string::npos)
			break;

		std::string name = header.substr(pos, eq - pos);
		size_t nameEnd = name.size();
		while (nameEnd > 0 && name[nameEnd - 1] == ' ')
			--nameEnd;
		name = name.substr(0, nameEnd);

		size_t semi = header.find(';', eq + 1);
		std::string value = header.substr(eq + 1,
			semi == std::string::npos ? std::string::npos : semi - eq - 1);

		if (!name.empty())
			req.cookies[name] = value;

		if (semi == std::string::npos)
			break;
		pos = semi + 1;
	}
}

HttpRequest RequestParser::parse() {
	HttpRequest parsedRequest;

	size_t pos = 0;
	_parseRequestLine(parsedRequest, pos);
	_parseHeaders(parsedRequest, pos);

	if (parsedRequest.version == "HTTP/1.1") {
		std::map<std::string, std::string>::const_iterator hostIt =
			parsedRequest.headers.find("Host");
		if (hostIt == parsedRequest.headers.end() || hostIt->second.empty())
			throw RequestParserException("HTTP/1.1 request missing Host header");
	}

	_parseBody(parsedRequest, pos);
	_parseCookies(parsedRequest);

	return parsedRequest;
}
