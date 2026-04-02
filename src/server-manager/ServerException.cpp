#include "ServerException.hpp"

ServerException::ServerException(const std::string& message)
	: _message(message)
{}

const char* ServerException::what() const throw() {
	return _message.c_str();
}
