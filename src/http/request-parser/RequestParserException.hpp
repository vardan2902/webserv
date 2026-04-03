#pragma once

#include <exception>
#include <string>

class RequestParserException : public std::exception {
private:
	std::string _message;

public:
	explicit RequestParserException(const std::string& message);
	virtual ~RequestParserException() throw() {}
	virtual const char* what() const throw();
};
