#pragma once

#include <exception>
#include <string>

class EventLoopException : public std::exception {
private:
	std::string _message;

public:
	explicit EventLoopException(const std::string& message);
	virtual ~EventLoopException() throw() {}
	virtual const char* what() const throw();
};
