#include "Logger.hpp"

Logger::Logger() {}

Logger::Logger(const Logger &) {}

Logger& Logger::operator=(const Logger &) {
	return *this;
}

Logger::~Logger() {};

static std::string _now() {
	time_t t = time(NULL);
	struct tm* tm_info = localtime(&t);
	char buf[32];
	strftime(buf, sizeof(buf), "[%d/%b/%Y:%H:%M:%S]", tm_info);
	return std::string(buf);
}

void Logger::info(const std::string& message) {
	std::cout << "\033[32m" << _now() << " " << message << "\033[0m" << std::endl;
}

void Logger::error(const std::string& message) {
	std::cerr << "\033[31m" << _now() << " [error] " << message << "\033[0m" << std::endl;
}

void Logger::debug(const std::string& message) {
	if (DEBUG_MODE)
		std::cout << "\033[35m" << _now() << " [debug] " << message << "\033[0m" << std::endl;
}