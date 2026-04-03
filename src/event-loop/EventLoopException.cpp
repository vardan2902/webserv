#include "EventLoopException.hpp"

EventLoopException::EventLoopException(const std::string& message)
	: _message(message)
{}

const char* EventLoopException::what() const throw() {
	return _message.c_str();
}
