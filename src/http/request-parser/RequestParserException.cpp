#include "RequestParserException.hpp"

RequestParserException::RequestParserException(const std::string& message)
	: _message(message)
{}

const char* RequestParserException::what() const throw() {
	return _message.c_str();
}
