#include <core/Logger.hpp>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>

namespace Log {

std::atomic<unsigned> Logger::currentLevel = 0;

static std::mutex logMutex;

const char *Logger::levelName(unsigned lvl)
{
	switch (lvl) {
		case ERROR:
			return "ERR";
		case WARN:
			return "WRN";
		case INFO:
			return "INF";
		case DEBUG:
			return "DBG";
		case TRACE:
			return "TRC";
		default:
			return "UNK";
	}
}

const char *Logger::levelColor(unsigned lvl)
{
	switch (lvl) {
		case ERROR:
			return "\033[31m"; // red
		case WARN:
			return "\033[33m"; // yellow
		case INFO:
			return "\033[32m"; // green
		case DEBUG:
			return "\033[36m"; // cyan
		case TRACE:
			return "\033[37m"; // gray
		default:
			return "\033[0m";
	}
}

void Logger::setLevel(unsigned lvl)
{
	currentLevel.store(lvl, std::memory_order_relaxed);
}

unsigned Logger::getLevel()
{
	return currentLevel.load(std::memory_order_relaxed);
}

bool Logger::enabled(unsigned lvl)
{
	return lvl <= currentLevel.load(std::memory_order_relaxed);
}

void Logger::log(unsigned lvl, const std::string &msg)
{
	if (!enabled(lvl))
		return;

	std::lock_guard<std::mutex> lock(logMutex);

	using clock = std::chrono::system_clock;
	auto now = clock::now();
	auto t = clock::to_time_t(now);
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

	std::tm tm{};
	localtime_r(&t, &tm);

	std::cerr << levelColor(lvl) << "[" << levelName(lvl) << "] " << std::put_time(&tm, "%F %T") << "." << std::setw(3)
			  << std::setfill('0') << ms.count() << " " << msg << "\033[0m" // reset color
			  << "\n";
}

} // namespace Log
