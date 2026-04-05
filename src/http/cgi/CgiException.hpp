#pragma once

#include <exception>
#include <string>

class CgiException : public std::exception {
private:
	std::string _message;

public:
	explicit CgiException(const std::string& message);
	virtual ~CgiException() throw() {}
	virtual const char* what() const throw();
};
