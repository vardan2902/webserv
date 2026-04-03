#pragma once

#include <exception>
#include <string>

class ParserException : public std::exception {
private:
    std::string _message;

public:
    explicit ParserException(const std::string& message);
	virtual ~ParserException() throw() {}
    virtual const char* what() const throw();
};
