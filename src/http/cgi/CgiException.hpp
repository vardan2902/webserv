#pragma once

#include <exception>
#include <string>

class CgiException : public std::exception {
private:
	std::string _message;

public:
	explicit CgiException(const std::string& message) : _message(message) {}
	CgiException(const CgiException& other) : _message(other._message) {}
	CgiException& operator=(const CgiException& other) {
		if (this != &other)
			_message = other._message;
		return *this;
	}
	virtual ~CgiException() throw() {}
	virtual const char* what() const throw() { return _message.c_str(); }
};
