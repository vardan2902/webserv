Val# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

A 42 school project implementing an HTTP web server in C++. Config file parsing is the first completed pipeline stage; HTTP server logic (sockets, request handling, response generation) is not yet implemented.

**Note:** The Makefile comment `# change to 98` indicates a planned switch from C++11 to C++98, which is a likely 42 school requirement.

## Build Commands

```bash
make          # Build ./webserv executable
make re       # Full rebuild (fclean + all)
make clean    # Remove obj/ directory
make fclean   # Remove obj/ and the webserv binary
```

**Run:** `./webserv <config_file>` (e.g., `./webserv configs/config_basic.conf`)

The Makefile auto-discovers all `.cpp` files under `src/` via `find`, so no manual source list maintenance is needed.

**Compiler flags:** `-Wall -Wextra -Werror -std=c++11 -I./src`

## Architecture

Three-stage pipeline for processing Nginx-like config files:

```
Config file text  →  [Tokenizer]  →  stream of Tokens
                  →  [Parser]     →  vector<Server> structs
                  →  [Validator]  →  semantic checks (throws on error)
                  →  [Web Server] →  (not yet implemented)
```

### Key Classes

**`Token`** (`src/tokenizer/Token/Token.hpp`) — Value object with `Type` enum (`Word`, `LBrace`, `RBrace`, `Semicolon`, `EndOfFile`), string `value`, and `line`/`column` for error reporting.

**`ITokenizer`** — Abstract interface with `tokenize()`, `getTokens()`, `nextToken()`, `peekToken()`, `hasNext()`. The `Parser` depends on this interface (not the concrete class) to support mocking.

**`Tokenizer : ITokenizer`** (`src/tokenizer/`) — Walks config text character-by-character, skips whitespace and `#`-comments, emits tokens.

**`Parser`** (`src/parser/`) — Holds `ITokenizer&`, builds `std::vector<Server>`. Handles `server { }` blocks with `listen`/`root` directives and nested `location { }` blocks. Uses `expect()` to consume/validate tokens, throwing `ParserException` on mismatch.

**`Validator`** (`src/validator/`) — Static utility class (private constructor). `Validator::validate(const std::vector<Server>&)` checks port range (1–65535), no duplicate ports, non-empty roots, location paths start with `/`.

**Data structures** (`src/config.hpp`):
```cpp
struct Location { std::string path, root, index; };
struct Server   { int port = 80; std::string root; std::vector<Location> locations; };
```

### Exception Hierarchy

Each pipeline layer throws its own typed exception (`ParserException`, `ValidationException`), all inheriting `std::exception`. `main.cpp` catches each separately.

## Config File Format

```nginx
server {
    listen 8080;
    root /var/www/html;

    location /api {
        root /var/www/api;
        index index.php;
    }
}
```

- `#` begins a line comment; statements end with `;`; blocks use `{ }`
- Supported server directives: `listen`, `root`
- Supported location directives: `root`, `index`

Test configs in `configs/` include valid examples and error cases (bad port, missing semicolon, unknown directive, duplicate ports).
