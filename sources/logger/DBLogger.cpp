#include "logger/DBLogger.hpp"

namespace Log {

bool DBLogger::ready()
{
	return db != nullptr;
}

void DBLogger::log(Level aLevel, std::string &msg)
{
	if (level < aLevel ) {
		return;
	}

	if (db) {
		db->insertText(static_cast<unsigned>(aLevel), msg);
	}
}

void DBLogger::setLevel(Level aLevel)
{
	level = aLevel;
}

void DBLogger::registerDB(std::shared_ptr<Database> aDb)
{
	db = aDb;
}

std::shared_ptr<Database> DBLogger::db;
Level DBLogger::level{Level::ERROR};

} // namespace Log
