#include "Tokenizer.hpp"

namespace {
    bool isSymbol(char c) {
        return c == BLOCK_OPEN || c == BLOCK_CLOSE || c == SEMICOLON;
    }

    Type symbolType(char c) {
        switch (c) {
            case BLOCK_OPEN: return LBrace;
            case BLOCK_CLOSE: return RBrace;
            case SEMICOLON: return Semicolon;
            default: return Word;
        }
    }

    void skipWhitespace(const std::string& input, size_t& pos, int& line, int& column) {
        while (pos < input.size()) {
            char c = input[pos];
            if (c == NEW_LINE) {
                line++;
                column = 1;
                pos++;
            }
            else if (std::isspace(c)) {
                column++;
                pos++;
            }
            else
                break;
        }
    }

    void skipComment(const std::string& input, size_t& pos, int& line, int& column) {
        while (pos < input.size() && input[pos] != NEW_LINE) {
            pos++;
            column++;
        }
        if (pos < input.size() && input[pos] == NEW_LINE) {
            line++;
            column = 1;
            pos++;
        }
    }

    std::string readWord(const std::string& input, size_t& pos, int& column) {
        std::string word;

        while (pos < input.size()) {
            char c = input[pos];

            if (std::isspace(c) || c == BLOCK_OPEN || c == BLOCK_CLOSE || c == SEMICOLON || c == HASHTAG)
                break;

            word += c;
            pos++;
            column++;
        }

        return word;
    }
}


Tokenizer::Tokenizer(const std::string& input)
    : _input(input), _pos(0)
{}
Tokenizer::Tokenizer(const Tokenizer& other)
    : _input(other._input), _pos(other._pos)
{}
Tokenizer& Tokenizer::operator=(const Tokenizer& other) {
    if (this != &other) {
        _input = other._input;
        _pos = other._pos;
    }
    return *this;
}
Tokenizer::~Tokenizer() {}

void Tokenizer::tokenize() {
    _tokens.clear();
    size_t pos = 0;
    int line = 1;
    int column = 1;

    while (pos < _input.size()) {
        skipWhitespace(_input, pos, line, column);

        if (pos >= _input.size())
            break;

        char c = _input[pos];

        if (c == HASHTAG) {
            skipComment(_input, pos, line, column);
            continue;
        }

        if (isSymbol(c)) {
            _tokens.push_back(Token(symbolType(c), std::string(1, c), line, column));
            pos++;
            column++;
            continue;
        }

        std::string word = readWord(_input, pos, column);
        if (!word.empty())
            _tokens.push_back(Token(Word, word, line, column - static_cast<int>(word.size())));
    }

    _tokens.push_back(Token(EndOfFile, EMPTY_STRING, line, column));
}

const std::vector<Token>& Tokenizer::getTokens() const {
    return _tokens;
}

Token Tokenizer::nextToken() {
    if (_pos < _tokens.size())
        return _tokens[_pos++];
    return Token(EndOfFile, EMPTY_STRING);
}

Token Tokenizer::peekToken() const {
    if (_pos < _tokens.size())
        return _tokens[_pos];
    return Token(EndOfFile, EMPTY_STRING);
}

bool Tokenizer::hasNext() const {
    return _pos < _tokens.size();
}
