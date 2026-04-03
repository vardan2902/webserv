#include "ListenerException.hpp"

ListenerException::ListenerException(const std::string& message)
	: _message(message)
{}

const char* ListenerException::what() const throw() {
	return _message.c_str();
}
