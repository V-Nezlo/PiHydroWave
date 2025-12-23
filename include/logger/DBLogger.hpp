#ifndef DBLOGGER_HPP
#define DBLOGGER_HPP

#include "core/Types.hpp"
#include <atomic>
#include <memory>
#include <storage/Database.hpp>
#include <string>

namespace Log {

class DBLogger {
public:
	static bool ready();
	static void log(Level aLevel, std::string &msg);
	static void setLevel(Level aLevel);
	static void registerDB(std::shared_ptr<Database> aDb);

private:
	static std::shared_ptr<Database> db;
	static Level level;
};

}

#endif // DBLOGGER_HPP
