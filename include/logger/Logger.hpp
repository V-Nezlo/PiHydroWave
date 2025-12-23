#pragma once

#include "core/Types.hpp"
#include <atomic>
#include <memory>
#include <string>

namespace Log {

class Logger {
public:
	static void setLevel(Level aLevel);
	static void log(Level aLevel, std::string msg);
};

} // namespace Log

#define HYDRO_LOG_ERROR(msg) Log::Logger::log(Log::Level::ERROR, (msg))
#define HYDRO_LOG_WARN(msg) Log::Logger::log(Log::Level::WARN, (msg))
#define HYDRO_LOG_INFO(msg) Log::Logger::log(Log::Level::INFO, (msg))
#define HYDRO_LOG_DEBUG(msg) Log::Logger::log(Log::Level::DEBUG, (msg))
#define HYDRO_LOG_TRACE(msg) Log::Logger::log(Log::Level::TRACE, (msg))

#define LOG_IF(level, msg)                                                                                             \
	do {                                                                                                               \
		if (Log::Logger::enabled(level))                                                                               \
			Log::Logger::log(level, msg);                                                                              \
	} while (0)
