#include "ValidationException.hpp"

ValidationException::ValidationException(const std::string& message)
    : _message(message)
{}

const char* ValidationException::what() const noexcept {
    return _message.c_str();
}