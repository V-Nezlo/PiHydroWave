#pragma once

#include <drogon/orm/DbClient.h>
#include <filesystem>
#include <iostream>
#include <any>
#include <string>

class Database {
	struct TelemetryValue {
		std::string type;
		double value;
	};
public:
	explicit Database(const std::string &dbPath);
	void insertAny(const std::string &key, const std::any &value);
	void insertText(unsigned level, const std::string &msg);

	std::vector<std::tuple<std::string, std::string, double, std::string>>
	queryHistory(const std::string &key,
				 const std::string &fromTs,
				 const std::string &toTs,
				 size_t limit = 1000);

private:
	drogon::orm::DbClientPtr dbClient;
	std::string dbFilePath;



	TelemetryValue convertAny(const std::any &a);
	void initSchema();
};
