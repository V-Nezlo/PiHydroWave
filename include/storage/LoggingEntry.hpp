#pragma once

#include "storage/Database.hpp"
#include "core/Blackboard.hpp"
#include <memory>
#include <string>

template<typename T>
class DataLogger : public AbstractEntryObserver {
public:
	DataLogger(std::string aName, std::shared_ptr<Blackboard> aBb, std::shared_ptr<Database> aDb):
		name{aName},
		bb{aBb},
		db{aDb}
	{
		bb->subscribe(name, this);
	}

	// AbstractEntryObserver interface
	void onEntryUpdated(std::string_view aEntry, const std::any &aValue) override
	{
		std::string entry = std::string(aEntry);
		db->insertAny(entry, aValue);
	}

private:
	std::string name;
	std::shared_ptr<Blackboard> bb;
	std::shared_ptr<Database> db;
};
