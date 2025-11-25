#pragma once
#include <core/Blackboard.hpp>
#include <storage/Database.hpp>
#include <storage/DatabaseEntry.hpp>

#include <memory>

struct DatabasePackage {
	DatabasePackage(std::shared_ptr<Blackboard> aBb, std::shared_ptr<Database> aDb) :
		pumpState{"pump.state", aBb, aDb}, waterLevel{"pump.waterLevel", aBb, aDb}
	{
	}

private:
	DatabaseEntry pumpState;
	DatabaseEntry waterLevel;
};
