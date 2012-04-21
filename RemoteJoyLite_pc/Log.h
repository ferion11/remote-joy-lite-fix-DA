#ifndef _LOG_H_
#define _LOG_H_

enum LOG_LEVEL {
	LOG_LEVEL_DEBUG,
	LOG_LEVEL_INFO,
	LOG_LEVEL_WARN,
	LOG_LEVEL_ERROR,
	LOG_LEVEL_FATAL,
};

class Logger {
public:
	Logger(const char* filename, int line);
	void operator()(LOG_LEVEL level, const char* format, ...);

private:
	const char* filename;
	int line;
};

#define LOG Logger(__FILE__, __LINE__)

#endif
