#include "Token.hpp"

Token::Token(Type type, const std::string& value, int l, int c)
    : type(type), value(value), line(l), column(c)
{}
Token::Token(const Token& other)
    : type(other.type), value(other.value), line(other.line), column(other.column)
{}
Token& Token::operator=(const Token& other) {
    if (this != &other) {
        type = other.type;
        value = other.value;
        line = other.line;
        column = other.column;
    }
    return *this;
}
Token::~Token() {}

bool Token::is(Type x) const {
    return type == x;
}