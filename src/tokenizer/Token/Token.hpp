#pragma once

#include <string>

#define EMPTY_STRING ""
#define DEFAULT_LINE 1
#define DEFAULT_COLUMN 1

enum Type { Word, LBrace, RBrace, Semicolon, EndOfFile };

class Token {
public:
    Type type;
    std::string value;
    int line;
    int column;

    Token(
        Type t = Word,
        const std::string& = EMPTY_STRING,
        int l = DEFAULT_LINE, 
        int c = DEFAULT_COLUMN
    );
    Token(const Token&);
    Token& operator=(const Token&);
    ~Token();

    bool is(Type) const;
};
