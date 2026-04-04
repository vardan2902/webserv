#pragma once
#include <string>

#include "IRequestParser.hpp"

class RequestParser : public IRequestParser {
private:
	std::string _raw;

	void _parseRequestLine(HttpRequest& req, size_t& pos) const;
	void _parseHeaders(HttpRequest& req, size_t& pos) const;
	void _parseBody(HttpRequest& req, size_t pos) const;
	std::string _readLine(size_t& pos) const;
	std::string _decodeChunked(size_t pos) const;
public:
	RequestParser();
	RequestParser(const RequestParser&);
	RequestParser& operator=(const RequestParser&);
	~RequestParser();

	HttpRequest parse();
	void feed(const std::string&);
};
