#include "Parser.hpp"

Parser::Parser(ITokenizer& tokenizer)
    : _tokenizer(tokenizer)
{}
Parser::Parser(const Parser& other)
    : _tokenizer(other._tokenizer), _servers(other._servers)
{}
Parser& Parser::operator=(const Parser& other) {
    if (this != &other) {
        _tokenizer = other._tokenizer;
        _servers =other._servers;
    }
    return *this;
}
Parser::~Parser() {}

void Parser::parse()
{
    while (_tokenizer.hasNext())
    {
        Token tok = _tokenizer.peekToken();

        if (tok.is(Word) && tok.value == SERVER_KEYWORD)
            parseServer();
        else if (tok.is(EndOfFile))
            break;
        else
            throw ParserException("Unexpected token at top level: " + tok.value);
    }
}

const std::vector<Server>& Parser::getServers() const
{
    return _servers;
}

void Parser::parseServer()
{
    expect(Word, SERVER_KEYWORD);
    expect(LBrace);

    Server server;

    while (true)
    {
        Token tok = _tokenizer.peekToken();

        if (tok.is(RBrace))
        {
            _tokenizer.nextToken();
            break;
        }
        else if (tok.is(Word) && tok.value == LOCATION_KEYWORD)
            parseLocation(server);
        else if (tok.is(Word))
            parseDirective(server);
        else
            throw ParserException("Unexpected token in server block: " + tok.value);
    }

    _servers.push_back(server);
}
void Parser::parseLocation(Server& server)
{
    expect(Word, LOCATION_KEYWORD);
    Token pathTok = expect(Word, EMPTY_STRING, "Expected location path after 'location'");
    
    Location loc;
    loc.path = pathTok.value;

    expect(LBrace, EMPTY_STRING, "Expected '{' after location path");

    while (true)
    {
        Token tok = _tokenizer.peekToken();

        if (tok.is(RBrace))
        {
            _tokenizer.nextToken();
            break;
        }
        else if (tok.is(Word))
            parseLocationDirective(loc);
        else
            throw ParserException("Unexpected token in location block: " + tok.value);
    }

    server.locations.push_back(loc);
}

void Parser::parseDirective(Server& server)
{
    Token keyTok = expect(Word, EMPTY_STRING, "Expected server directive");
    Token valueTok = expect(Word, EMPTY_STRING, "Expected value for directive '" + keyTok.value + "'");
    expect(Semicolon, EMPTY_STRING, "Missing semicolon for directive '" + keyTok.value + "'");

    const std::string& key = keyTok.value;
    const std::string& value = valueTok.value;

    if (key == LISTEN_DIRECTIVE)
    {
        try
        {
            server.port = std::stoi(value);
        }
        catch (...)
        {
            throw ParserException("Invalid port value: " + value);
        }
    }
    else if (key == ROOT_DIRECTIVE)
        server.root = value;
    else
        throw ParserException("Unknown server directive: " + key);
}

void Parser::parseLocationDirective(Location& loc)
{
    Token keyTok   = expect(Word, EMPTY_STRING, "Expected location directive");
    Token valueTok = expect(Word, EMPTY_STRING, "Expected value for location directive '" + keyTok.value + "'");
    expect(Semicolon, EMPTY_STRING, "Missing semicolon for location directive '" + keyTok.value + "'");

    const std::string& key   = keyTok.value;
    const std::string& value = valueTok.value;

    if (key == ROOT_DIRECTIVE)
        loc.root = value;
    else if (key == INDEX_DIRECTIVE)
        loc.index = value;
    else
        throw ParserException("Unknown location directive: " + key);
}

Token Parser::expect(Type type, const std::string& expectedValue, const std::string& errorMessage)
{
    Token tok = _tokenizer.nextToken();

    if (!tok.is(type) || (!expectedValue.empty() && tok.value != expectedValue))
    {
        if (!errorMessage.empty())
            throw ParserException(errorMessage + ", got: " + tok.value);
        else if (!expectedValue.empty())
            throw ParserException("Expected token '" + expectedValue + "', got: " + tok.value);
        else
            throw ParserException("Unexpected token: " + tok.value);
    }

    return tok;
}