#pragma once

#include <exception>
#include <string>

class ListenerException : public std::exception {
private:
	std::string _message;

public:
	explicit ListenerException(const std::string& message);
	virtual const char* what() const noexcept;
};
