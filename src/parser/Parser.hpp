#pragma once

#include <vector>
#include <string>
#include <cstdlib>

#include "../config.hpp"
#include "../tokenizer/Token/Token.hpp"
#include "../tokenizer/ITokenizer.hpp"
#include "ParserException.hpp"

#define EMPTY_STRING                    ""
#define SERVER_KEYWORD                  "server"
#define LOCATION_KEYWORD                "location"
#define LISTEN_DIRECTIVE                "listen"
#define ROOT_DIRECTIVE                  "root"
#define INDEX_DIRECTIVE                 "index"
#define ALLOW_METHODS_DIRECTIVE         "allow_methods"
#define ERROR_PAGE_DIRECTIVE            "error_page"
#define CLIENT_MAX_BODY_SIZE_DIRECTIVE  "client_max_body_size"
#define RETURN_DIRECTIVE                "return"
#define AUTOINDEX_DIRECTIVE             "autoindex"
#define UPLOAD_STORE_DIRECTIVE          "upload_store"
#define CGI_EXT_DIRECTIVE               "cgi_ext"

class Parser {
private:
    ITokenizer& _tokenizer;
    std::vector<Server> _servers;

    void parseServer();
    void parseLocation(Server& server);
    void parseDirective(Server& server);
    void parseLocationDirective(Location& location);

    // Server directive handlers
    void _parseListen(Server&);
    void _parseServerRoot(Server&);
    void _parseClientMaxBodySize(Server&);
    void _parseErrorPage(Server&);

    // Location directive handlers
    void _parseLocationRoot(Location&);
    void _parseLocationIndex(Location&);
    void _parseAutoindex(Location&);
    void _parseAllowMethods(Location&);
    void _parseReturn(Location&);
    void _parseUploadStore(Location&);
    void _parseCgiExt(Location&);

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
