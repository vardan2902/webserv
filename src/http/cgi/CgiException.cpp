#include "CgiException.hpp"

CgiException::CgiException(const std::string& message)
	: _message(message)
{}

const char* CgiException::what() const throw() {
	return _message.c_str();
}
