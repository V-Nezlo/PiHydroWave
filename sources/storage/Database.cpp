#include "storage/Database.hpp"

Database::Database(const std::string &dbPath)
{
	namespace fs = std::filesystem;

	// Делаем абсолютный путь
	fs::path abs = fs::absolute(dbPath);
	dbFilePath = abs.string();

	std::cout << "[DB] Path: " << dbFilePath << "\n";
	std::cout << "[DB] CWD:  " << fs::current_path() << "\n";

	std::string connStr = "filename=" + dbFilePath;

	try {
		dbClient = drogon::orm::DbClient::newSqlite3Client(
			connStr,
			1
			);
	} catch (const std::exception &e) {
		std::cerr << "[DB] Failed to create client: " << e.what() << "\n";
		throw;
	}

	initSchema();
}

void Database::insertAny(const std::string &key, const std::any &value)
{
	try {
		auto tv = convertAny(value);

		dbClient->execSqlSync(
			"INSERT INTO telemetry (key, type, value, timestamp)"
			"VALUES (?, ?, ?, datetime('now'))",
			key,
			tv.type,
			tv.value
			);

		std::cout << "[DB] Insert telemetry OK\n";
	}
	catch (const std::exception &e) {
		std::cerr << "[DB] Insert telemetry ERROR: " << e.what() << "\n";
	}
}

std::vector<std::tuple<std::string, std::string, double, std::string> > Database::queryHistory(const std::string &key, const std::string &fromTs, const std::string &toTs, size_t limit)
{
	std::vector<std::tuple<std::string, std::string, double, std::string>> out;

	try {
		auto result = dbClient->execSqlSync(
			"SELECT key, type, value, timestamp "
			"FROM telemetry "
			"WHERE key = ? "
			"AND timestamp >= ? "
			"AND timestamp <= ? "
			"ORDER BY timestamp ASC "
			"LIMIT ?",
			key, fromTs, toTs, static_cast<int>(limit)
			);

		for (auto &row : result) {
			out.emplace_back(
				row["key"].as<std::string>(),
				row["type"].as<std::string>(),
				row["value"].as<double>(),
				row["timestamp"].as<std::string>()
				);
		}
	}
	catch (const drogon::orm::DrogonDbException &e) {
		std::cerr << "[DB] queryHistory ERROR: " << e.base().what() << "\n";
	}

	return out;
}

Database::TelemetryValue Database::convertAny(const std::any &a)
{
	if (a.type() == typeid(bool))
		return {"bool", std::any_cast<bool>(a) ? 1.0 : 0.0};

	if (a.type() == typeid(int))
		return {"int", static_cast<double>(std::any_cast<int>(a))};

	if (a.type() == typeid(uint32_t))
		return {"u32", static_cast<double>(std::any_cast<uint32_t>(a))};

	if (a.type() == typeid(uint64_t)) {
		uint64_t v = std::any_cast<uint64_t>(a);

		// Возможна потеря точности
		return {"u64", static_cast<double>(v)};
	}

	if (a.type() == typeid(double))
		return {"double", std::any_cast<double>(a)};

	throw std::runtime_error("Unsupported std::any type");
}

void Database::initSchema()
{
	try {
		dbClient->execSqlSync(
			"CREATE TABLE IF NOT EXISTS telemetry ("
			" id INTEGER PRIMARY KEY AUTOINCREMENT,"
			" key TEXT NOT NULL,"
			" type TEXT NOT NULL,"
			" value REAL NOT NULL,"
			" timestamp TEXT NOT NULL"
			");"
			);

		std::cout << "[DB] Schema OK\n";
	}
	catch (const drogon::orm::DrogonDbException &e) {
		std::cerr << "[DB] Schema ERROR: " << e.base().what() << "\n";
	}
}
