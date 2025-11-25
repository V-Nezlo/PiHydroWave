#pragma once

#include <atomic>
#include <string>

namespace Log {

enum Level : unsigned { ERROR = 0, WARN = 1, INFO = 2, DEBUG = 3, TRACE = 4 };

class Logger {
public:
	static void setLevel(unsigned lvl);
	static unsigned getLevel();
	static bool enabled(unsigned lvl);

	static void log(unsigned lvl, const std::string &msg);

private:
	static std::atomic<unsigned> currentLevel;

	static const char *levelName(unsigned lvl);
	static const char *levelColor(unsigned lvl);
};

} // namespace Log

#define LOG_ERROR(msg) Log::Logger::log(Log::ERROR, (msg))
#define LOG_WARN(msg) Log::Logger::log(Log::WARN, (msg))
#define LOG_INFO(msg) Log::Logger::log(Log::INFO, (msg))
#define LOG_DEBUG(msg) Log::Logger::log(Log::DEBUG, (msg))
#define LOG_TRACE(msg) Log::Logger::log(Log::TRACE, (msg))

#define LOG_IF(level, msg)                                                                                             \
	do {                                                                                                               \
		if (Log::Logger::enabled(level))                                                                               \
			Log::Logger::log(level, msg);                                                                              \
	} while (0)
