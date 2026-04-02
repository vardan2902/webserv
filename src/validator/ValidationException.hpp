#pragma once

#include <exception>
#include <string>

class ValidationException : public std::exception {
private:
    std::string _message;

public:
    explicit ValidationException(const std::string& message);
	virtual ~ValidationException() throw() {}
    virtual const char* what() const throw();
};
