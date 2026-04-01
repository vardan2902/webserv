#pragma once

#include <exception>
#include <string>

class RequestParserException : public std::exception {
private:
	std::string _message;

public:
	explicit RequestParserException(const std::string& message);
	virtual const char* what() const noexcept;
};
