#include "logger/SystemLogger.hpp"
#include "core/Types.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>

namespace Log {

void SystemLogger::setLevel(Level aLevel)
{
	currentLevel.store(aLevel, std::memory_order_relaxed);
}

Level SystemLogger::getLevel()
{
	return currentLevel.load(std::memory_order_relaxed);
}

bool SystemLogger::enabled(Level lvl)
{
	return lvl <= currentLevel.load(std::memory_order_relaxed);
}

void SystemLogger::log(Level aLevel, const std::string &msg)
{

	if (!enabled(aLevel))
		return;

	std::lock_guard<std::mutex> lock(logMutex);

	using clock = std::chrono::system_clock;
	auto now = clock::now();
	auto t = clock::to_time_t(now);
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

	std::tm tm{};
	localtime_r(&t, &tm);

	const unsigned lvl = static_cast<unsigned>(aLevel);

	std::cerr << levelColor(lvl) << "[" << levelName(lvl) << "] " << std::put_time(&tm, "%F %T") << "." << std::setw(3)
			  << std::setfill('0') << ms.count() << " " << msg << "\033[0m" // reset color
			  << "\n";
}

const char *SystemLogger::levelName(unsigned int lvl)
{
	const Level lev = static_cast<Level>(lvl);
	switch (lev) {
		case Level::ERROR:
			return "ERR";
		case Level::WARN:
			return "WRN";
		case Level::INFO:
			return "INF";
		case Level::DEBUG:
			return "DBG";
		case Level::TRACE:
			return "TRC";
		default:
			return "UNK";
	}
}

const char *SystemLogger::levelColor(unsigned int lvl)
{
	const Level lev = static_cast<Level>(lvl);

	switch (lev) {
		case Level::ERROR:
			return "\033[31m"; // red
		case Level::WARN:
			return "\033[33m"; // yellow
		case Level::INFO:
			return "\033[32m"; // green
		case Level::DEBUG:
			return "\033[36m"; // cyan
		case Level::TRACE:
			return "\033[37m"; // gray
		default:
			return "\033[0m";
	}
}

std::atomic<Level> SystemLogger::currentLevel = Level::ERROR;
std::mutex SystemLogger::logMutex{};

} // namespace Log
