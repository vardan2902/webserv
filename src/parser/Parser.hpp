#pragma once

#include <vector>
#include <string>
#include <cstdlib>

#include "../config.hpp"
#include "../tokenizer/Token/Token.hpp"
#include "../tokenizer/ITokenizer.hpp"
#include "ParserException.hpp"

#define EMPTY_STRING ""
#define SERVER_KEYWORD "server"
#define LOCATION_KEYWORD "location"
#define LISTEN_DIRECTIVE "listen"
#define ROOT_DIRECTIVE "root"
#define INDEX_DIRECTIVE "index"

class Parser
{
private:
    ITokenizer& _tokenizer;
    std::vector<Server> _servers;

    void parseServer();
    void parseLocation(Server& server);
    void parseDirective(Server& server);
    void parseLocationDirective(Location& location);
    Token expect(
        Type type,
        const std::string& expectedValue = EMPTY_STRING,
        const std::string& errorMessage = EMPTY_STRING
    );

public:
    Parser(ITokenizer&);
    Parser(const Parser&);
    Parser& operator=(const Parser&);
    ~Parser();

    void parse();
    const std::vector<Server>& getServers() const;
};
