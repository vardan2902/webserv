*This project has been created as part of the 42 curriculum by vapetros, ysaroyan, ggevorgi.*

---

## Description

**webserv** is a non-blocking HTTP/1.1 server written in C++98, built as part of the 42 school curriculum. It follows the design principles of NGINX — a single event loop drives all I/O using `epoll`, with no blocking reads or writes anywhere in the codebase.

The server supports:
- GET, POST, and DELETE HTTP methods
- Static file serving with 14 MIME types
- File uploads via `multipart/form-data`
- CGI execution by file extension (Python, PHP, or any interpreter)
- Cookie-based session management
- Autoindex directory listings
- Custom error pages and HTTP redirects
- Multiple virtual servers on separate interface:port pairs

Configuration syntax is modelled after NGINX's `server {}` block format.

---

## Instructions

### Requirements

- Linux (uses `epoll`) or macOS (uses `fcntl` with `O_NONBLOCK`)
- `c++` compiler supporting `-std=c++98`
- `make`

### Build

```bash
make          # build ./webserv
make re       # full rebuild
make fclean   # remove binary and object files
```

### Docker

```bash
make docker-build   # build Docker image
make docker-run     # run container (exposes port 8080)
```

### Run

```bash
./webserv <configuration_file>

# Examples
./webserv configs/basic/config.conf               # minimal config
./webserv configs/full/config.conf                # all directives
./webserv configs/bad_port/config.conf            # exits with error (port out of range)
./webserv configs/duplicate_host_port/config.conf # exits with error (duplicate host:port)
```

### Configuration file format

```nginx
server {
    listen 127.0.0.1:8080;
    root /var/www/html;
    client_max_body_size 10m;
    error_page 404 /404.html;

    location / {
        index index.html;
        allow_methods GET;
    }

    location /upload {
        allow_methods POST;
        upload_store /var/www/uploads;
    }

    location /cgi-bin {
        allow_methods GET POST;
        cgi_ext .py /usr/bin/python3;
        cgi_ext .php /usr/bin/php-cgi;
    }

    location /files {
        autoindex on;
        allow_methods GET DELETE;
    }

    location /old {
        return 301 http://localhost:8080/;
    }
}
```

**Supported directives:** `listen`, `root`, `index`, `autoindex`, `allow_methods`, `return`, `error_page`, `client_max_body_size`, `upload_store`, `cgi_ext`

---

## Resources

### HTTP / Networking

- [RFC 7230 — HTTP/1.1 Message Syntax and Routing](https://datatracker.ietf.org/doc/html/rfc7230)
- [RFC 7231 — HTTP/1.1 Semantics and Content](https://datatracker.ietf.org/doc/html/rfc7231)
- [RFC 3875 — The Common Gateway Interface (CGI/1.1)](https://datatracker.ietf.org/doc/html/rfc3875)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- [NGINX documentation — server block configuration](https://nginx.org/en/docs/http/ngx_http_core_module.html)
- [epoll(7) man page](https://man7.org/linux/man-pages/man7/epoll.7.html)
- [MDN — HTTP overview](https://developer.mozilla.org/en-US/docs/Web/HTTP/Overview)

### AI Usage

Claude (Anthropic) was used during this project for the following tasks:

- **Architecture design** — reviewing the overall structure (event loop state machine, DI container layout, config pipeline)
- **Debugging** — diagnosing non-obvious bugs in chunked transfer decoding and CGI pipe handling
- **Code review** — checking C++98 compliance (no `auto`, no range-for, no `unordered_map`) and identifying grade-0 traps from the subject
- **Documentation** — generating and maintaining `STATUS.md`, `CLAUDE.md`, and the Obsidian vault notes

AI was not used to write core implementation logic directly; all algorithmic decisions and system design were made by the team.
