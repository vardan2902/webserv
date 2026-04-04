#pragma once

#include <exception>
#include <string>

class DIException : public std::exception {
private:
    std::string _message;

public:
    explicit DIException(const std::string& message) : _message(message) {}
    virtual ~DIException() throw() {}

    virtual const char* what() const throw() {
        return _message.c_str();
    }
};
