#pragma once

#include <cctype>
#include <vector>

#include "Token/Token.hpp"

class ITokenizer
{
public:
    virtual void tokenize() = 0;
    virtual Token nextToken() = 0;
    virtual Token peekToken() const = 0;
    virtual bool hasNext() const = 0;
};
