#pragma once

#include <vector>

#include "ITokenizer.hpp"

#define BLOCK_OPEN '{'
#define BLOCK_CLOSE '}'
#define SEMICOLON ';'
#define HASHTAG '#'
#define NEW_LINE '\n'
#define EMPTY_STRING ""

class Tokenizer : public ITokenizer
{
private:
    std::string _input;
    std::vector<Token> _tokens;
    std::size_t _pos;

public:
    Tokenizer(const std::string& input);
    Tokenizer(const Tokenizer&);
    Tokenizer& operator=(const Tokenizer&);
    ~Tokenizer();

    void tokenize();
    const std::vector<Token>& getTokens() const;

    Token nextToken();
    Token peekToken() const;
    bool hasNext() const;
};
