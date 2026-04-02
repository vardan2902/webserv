#include "ParserException.hpp"

ParserException::ParserException(const std::string& message)
    : _message(message)
{}

const char* ParserException::what() const throw() {
    return _message.c_str();
}
