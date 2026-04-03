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

void Parser::parse() {
    while (_tokenizer.hasNext()) {
        Token tok = _tokenizer.peekToken();

        if (tok.is(Word) && tok.value == SERVER_KEYWORD)
            parseServer();
        else if (tok.is(EndOfFile))
            break;
        else
            throw ParserException("Unexpected token at top level: " + tok.value);
    }
}

const std::vector<Server>& Parser::getServers() const {
    return _servers;
}

void Parser::parseServer() {
    expect(Word, SERVER_KEYWORD);
    expect(LBrace);

    Server server;

    while (true) {
        Token tok = _tokenizer.peekToken();

        if (tok.is(RBrace)) {
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

void Parser::parseLocation(Server& server) {
    expect(Word, LOCATION_KEYWORD);
    Token pathTok = expect(Word, EMPTY_STRING, "Expected location path after 'location'");

    Location loc;
    loc.path = pathTok.value;

    expect(LBrace, EMPTY_STRING, "Expected '{' after location path");

    while (true) {
        Token tok = _tokenizer.peekToken();

        if (tok.is(RBrace)) {
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

// ─── helpers ────────────────────────────────────────────────────────────────

static int _toInt(const std::string& s) {
    char* end;
    long val = std::strtol(s.c_str(), &end, 10);
    if (end == s.c_str() || *end != '\0')
        throw ParserException("Invalid integer value: " + s);
    return static_cast<int>(val);
}

// ─── server directive handlers ───────────────────────────────────────────────

void Parser::_parseListen(Server& server) {
    Token valueTok = expect(Word, EMPTY_STRING, "Expected port for 'listen'");
    expect(Semicolon, EMPTY_STRING, "Missing semicolon for 'listen'");
    try {
        server.port = _toInt(valueTok.value);
    } catch (...) {
        throw ParserException("Invalid port value: " + valueTok.value);
    }
}

void Parser::_parseServerRoot(Server& server) {
    Token valueTok = expect(Word, EMPTY_STRING, "Expected path for 'root'");
    expect(Semicolon, EMPTY_STRING, "Missing semicolon for 'root'");
    server.root = valueTok.value;
}

void Parser::_parseClientMaxBodySize(Server& server) {
    Token valueTok = expect(Word, EMPTY_STRING, "Expected value for 'client_max_body_size'");
    expect(Semicolon, EMPTY_STRING, "Missing semicolon for 'client_max_body_size'");
    const std::string& value = valueTok.value;
    char* end;
    long size = std::strtol(value.c_str(), &end, 10);
    if (end == value.c_str())
        throw ParserException("Invalid client_max_body_size value: " + value);
    if (*end == 'k' || *end == 'K') size *= 1024;
    else if (*end == 'm' || *end == 'M') size *= 1024 * 1024;
    else if (*end == 'g' || *end == 'G') size *= 1024 * 1024 * 1024;
    server.clientMaxBodySize = static_cast<size_t>(size);
}

void Parser::_parseErrorPage(Server& server) {
    std::vector<Token> tokens;
    while (_tokenizer.peekToken().is(Word))
        tokens.push_back(_tokenizer.nextToken());
    if (tokens.size() < 2)
        throw ParserException("error_page requires at least one code and a path");
    expect(Semicolon, EMPTY_STRING, "Missing semicolon for 'error_page'");
    std::string path = tokens.back().value;
    for (size_t i = 0; i < tokens.size() - 1; ++i)
        server.errorPages[_toInt(tokens[i].value)] = path;
}

void Parser::parseDirective(Server& server) {
    Token keyTok = expect(Word, EMPTY_STRING, "Expected server directive");
    const std::string& key = keyTok.value;

    typedef void (Parser::*ServerHandler)(Server&);
    typedef std::map<std::string, ServerHandler> ServerHandlerMap;

    static ServerHandlerMap handlers;
    if (handlers.empty()) {
        handlers[LISTEN_DIRECTIVE]               = &Parser::_parseListen;
        handlers[ROOT_DIRECTIVE]                 = &Parser::_parseServerRoot;
        handlers[CLIENT_MAX_BODY_SIZE_DIRECTIVE] = &Parser::_parseClientMaxBodySize;
        handlers[ERROR_PAGE_DIRECTIVE]           = &Parser::_parseErrorPage;
    }

    ServerHandlerMap::iterator it = handlers.find(key);
    if (it == handlers.end())
        throw ParserException("Unknown server directive: " + key);
    (this->*it->second)(server);
}

// ─── location directive handlers ─────────────────────────────────────────────

void Parser::_parseLocationRoot(Location& loc) {
    Token valueTok = expect(Word, EMPTY_STRING, "Expected path for 'root'");
    expect(Semicolon, EMPTY_STRING, "Missing semicolon for 'root'");
    loc.root = valueTok.value;
}

void Parser::_parseLocationIndex(Location& loc) {
    Token valueTok = expect(Word, EMPTY_STRING, "Expected file for 'index'");
    expect(Semicolon, EMPTY_STRING, "Missing semicolon for 'index'");
    loc.index = valueTok.value;
}

void Parser::_parseAutoindex(Location& loc) {
    Token valueTok = expect(Word, EMPTY_STRING, "Expected 'on' or 'off' for 'autoindex'");
    expect(Semicolon, EMPTY_STRING, "Missing semicolon for 'autoindex'");
    const std::string& value = valueTok.value;
    if (value == "on")        loc.autoindex = true;
    else if (value == "off")  loc.autoindex = false;
    else throw ParserException("Invalid autoindex value: " + value + " (expected 'on' or 'off')");
}

void Parser::_parseAllowMethods(Location& loc) {
    while (_tokenizer.peekToken().is(Word))
        loc.allowMethods.push_back(_tokenizer.nextToken().value);
    expect(Semicolon, EMPTY_STRING, "Missing semicolon for 'allow_methods'");
}

void Parser::_parseReturn(Location& loc) {
    Token codeTok = expect(Word, EMPTY_STRING, "Expected status code for 'return'");
    Token urlTok  = expect(Word, EMPTY_STRING, "Expected URL for 'return'");
    expect(Semicolon, EMPTY_STRING, "Missing semicolon for 'return'");
    try { loc.returnCode = _toInt(codeTok.value); }
    catch (...) { throw ParserException("Invalid return code: " + codeTok.value); }
    loc.returnUrl = urlTok.value;
}

void Parser::_parseUploadStore(Location& loc) {
    Token valueTok = expect(Word, EMPTY_STRING, "Expected path for 'upload_store'");
    expect(Semicolon, EMPTY_STRING, "Missing semicolon for 'upload_store'");
    loc.uploadStore = valueTok.value;
}

void Parser::parseLocationDirective(Location& loc) {
    Token keyTok = expect(Word, EMPTY_STRING, "Expected location directive");
    const std::string& key = keyTok.value;

    typedef void (Parser::*LocationHandler)(Location&);
    typedef std::map<std::string, LocationHandler> LocationHandlerMap;

    static LocationHandlerMap handlers;
    if (handlers.empty()) {
        handlers[ROOT_DIRECTIVE]          = &Parser::_parseLocationRoot;
        handlers[INDEX_DIRECTIVE]         = &Parser::_parseLocationIndex;
        handlers[AUTOINDEX_DIRECTIVE]     = &Parser::_parseAutoindex;
        handlers[ALLOW_METHODS_DIRECTIVE] = &Parser::_parseAllowMethods;
        handlers[RETURN_DIRECTIVE]        = &Parser::_parseReturn;
        handlers[UPLOAD_STORE_DIRECTIVE]  = &Parser::_parseUploadStore;
    }

    LocationHandlerMap::iterator it = handlers.find(key);
    if (it == handlers.end())
        throw ParserException("Unknown location directive: " + key);
    (this->*it->second)(loc);
}

// ─── token helpers ───────────────────────────────────────────────────────────

Token Parser::expect(Type type, const std::string& expectedValue, const std::string& errorMessage) {
    Token tok = _tokenizer.nextToken();

    if (!tok.is(type) || (!expectedValue.empty() && tok.value != expectedValue)) {
        if (!errorMessage.empty())
            throw ParserException(errorMessage + ", got: " + tok.value);
        else if (!expectedValue.empty())
            throw ParserException("Expected token '" + expectedValue + "', got: " + tok.value);
        else
            throw ParserException("Unexpected token: " + tok.value);
    }

    return tok;
}
