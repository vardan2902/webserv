#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
// #include <sys/epoll.h>
#include <sys/socket.h>

#include "tokenizer/Tokenizer.hpp"
#include "parser/Parser.hpp"
#include "parser/ParserException.hpp"
#include "validator/Validator.hpp"
#include "validator/ValidationException.hpp"

#define DEBUG_MODE 1
