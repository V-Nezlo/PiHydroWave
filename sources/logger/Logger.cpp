#include "logger/Logger.hpp"
#include "logger/DBLogger.hpp"
#include "logger/SystemLogger.hpp"
#include "logger/WebSocketLogger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>

namespace Log {

void Logger::setLevel(Level aLevel)
{
	DBLogger::setLevel(aLevel);
	SystemLogger::setLevel(aLevel);
	WebSocketLogger::setLevel(aLevel);
}

void Logger::log(Level aLevel, std::string msg)
{
	DBLogger::log(aLevel, msg);
	SystemLogger::log(aLevel, msg);
	WebSocketLogger::log(aLevel, msg);
}

} // namespace Log
