#pragma once

#include <exception>
#include <string>

class ServerException : public std::exception {
private:
	std::string _message;

public:
	explicit ServerException(const std::string& message);
	virtual const char* what() const noexcept;
};
