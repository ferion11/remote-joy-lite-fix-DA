#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include "Log.h"

using namespace std;

#define ENABLE_LOG
//#undef ENABLE_LOG

static const LOG_LEVEL LOG_LEVEL_THRESHOLD = LOG_LEVEL_WARN;

struct File {
	FILE* file;
	File() {
		char filename[64];
		sprintf(filename, "remote_joy_lite_log.txt");
		file = fopen(filename, "w");
	}
	virtual ~File() {
		if (file) {
			fclose(file);
			file = NULL;
		}
	}
};

#ifdef ENABLE_LOG
static File file;
#endif

Logger::Logger(const char* filename, int line) : filename(filename), line(line) { }

void Logger::operator()(LOG_LEVEL level, const char* format, ...) {
#ifdef ENABLE_LOG

	va_list ap;
	va_start(ap, format);
	char buffer[256];
	vsnprintf(buffer, sizeof(buffer) - 1, format, ap);
	va_end(ap);

	if (level < LOG_LEVEL_THRESHOLD) {
		return;
	}

	time_t t = time(NULL);
	char timeString[32];
	strcpy(timeString, ctime(&t));
	for (int i = strlen(timeString) - 1; i >= 0 && isspace(timeString[i]); --i) {
		timeString[i] = '\0';
	}

	fprintf(file.file, "%s [%s](%d) %s\n", timeString, filename, line, buffer);
	fflush(file.file);
#endif
}
