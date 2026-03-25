#include "Logger.hpp"

#include <iostream>
#include <ostream>

Logger::Logger() {}

Logger::Logger(const Logger &) {}

Logger& Logger::operator=(const Logger &) {
	return *this;
}

Logger::~Logger() {};

void Logger::info(const std::string& message) {
	std::cout << "\033[32m" << message << "\033[0m" << std::endl;
}

void Logger::error(const std::string& message) {
	std::cerr << "\033[31m" << message << "\033[0m" << std::endl;
}

void Logger::debug(const std::string& message) {
	if (DEBUG_MODE)
		std::cout << "\033[35m" << message << "\033[0m" << std::endl;
}