#include "ILogger.hpp"
#include "../webserv.hpp"

class Logger : public ILogger {
public:
	Logger();
	Logger(const Logger&);
	Logger& operator=(const Logger&);
	~Logger();

	void info(const std::string&);
	void error(const std::string&);
	void debug(const std::string&);
};
