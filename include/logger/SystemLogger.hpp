#ifndef SYSTEMLOGGER_HPP
#define SYSTEMLOGGER_HPP

#include "core/Types.hpp"
#include <atomic>
#include <memory>
#include <string>

namespace Log {

class SystemLogger {
public:
	static void setLevel(Level aLevel);
	static Level getLevel();
	static bool enabled(Level lvl);
	static void log(Level aLevel, const std::string &msg);

private:
	static std::atomic<Level> currentLevel;
	static std::mutex logMutex;

	static const char *levelName(unsigned lvl);
	static const char *levelColor(unsigned lvl);
};

} // namespace Log

#endif // SYSTEMLOGGER_HPP
