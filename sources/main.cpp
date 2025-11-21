
#include "LampController.hpp"
#include "PumpController.hpp"
#include "RadioHandler.hpp"
#include "SerialEspProxy.hpp"
#include "SettingsManager.hpp"

#include "BackRestController.hpp"
#include "BackWebSocket.hpp"

#include "core/Blackboard.hpp"
#include "core/EventBus.hpp"
#include "core/TimeWrapper.hpp"
#include "core/Types.hpp"
#include "storage/LoggingEntry.hpp"
#include "storage/Database.hpp"

#include <UtilitaryRS/Crc64.hpp>
#include <UtilitaryRS/Crc8.hpp>

#include <UtilitaryRS/DeviceHub.hpp>
#include <UtilitaryRS/RsTypes.hpp>

#include <chrono>
#include <drogon/HttpAppFramework.h>
#include <drogon/orm/DbClient.h>
#include <iostream>
#include <csignal>
#include <memory>
#include <thread>

using Hub = RS::DeviceHub<10, SerialEspProxy, TimeWrapper, Crc8, Crc64, 256>;

int main()
{
	auto blackBoard = std::make_shared<Blackboard>();
	SettingsManager config(blackBoard, "config.json");
	auto bus = std::make_shared<EventBus>();

	config.registerSetting("config.lamp.enabled", SettingType::BOOL, true, "Enable lamp");
	config.registerSetting("config.lamp.onTime", SettingType::UNSIGNED, 0, "Lamp on time in minutes");
	config.registerSetting("config.lamp.offTime", SettingType::UNSIGNED, 0, "Lamp off time in minutes");

	config.registerSetting("config.pump.enabled", SettingType::BOOL, true, "Enable pump");
	config.registerSetting("config.pump.mode", SettingType::UNSIGNED, 0, "Pump mode");
	config.registerSetting("config.pump.onTime", SettingType::UNSIGNED, 0, "Pump on time in seconds");
	config.registerSetting("config.pump.offTime", SettingType::UNSIGNED, 0, "Pump off time in seconds");
	config.registerSetting("config.pump.swingTime", SettingType::UNSIGNED, 0, "Pump swing time in seconds");
	config.registerSetting("config.pump.validTime", SettingType::UNSIGNED, 0, "Upper validation time in seconds");
	config.registerSetting("config.pump.maxFloodTime", SettingType::UNSIGNED, 0, "Max time for tank flooding in secs");
	config.registerSetting("config.pump.minWaterLevel", SettingType::UNSIGNED, 0, "Minimal water level in percent for pump operation");

	config.registerSetting("slaves.mac.1", SettingType::U64, 0xaabbccddeeUL, "MAC SLOT 1");
	config.registerSetting("slaves.mac.2", SettingType::U64, 0xaabbccddeeUL, "MAC SLOT 2");

	if (!config.load()) {
		std::cerr << "Failed to initialize config" << std::endl;
		return 1;
	}

	blackBoard->set("config.pump.enabled", false);

	auto pumpEnabled = blackBoard->get<bool>("config.pump.enabled1");
	if (pumpEnabled) {
		std::cout << "config.pump.enabled: " << *pumpEnabled << std::endl;
	}

//	try {
//		std::string key("water");
//		double value = 123.22;

//		Database::init();
//		Database::getDb()->execSqlSync(
//			"INSERT INTO telemetry (key, value, timestamp) VALUES (?, ?, datetime('now'))",
//			key,
//			value
//			);
//		std::cout << "INSERT done\n";
//	} catch (const drogon::orm::DrogonDbException &e) {
//		std::cerr << "DB EXCEPTION: " << e.base().what() << "\n";
//	}

	Database db("hydroponic.db");
	std::any value = true;

	db.insertAny("pump.state", value);
	return 0;

	std::cout << "valera" << std::endl;

//	RS::DeviceVersion ver;
//	RadioHandler<Hub> radioHandler{"/dev/ttyUSB0", ver, blackBoard, bus};
//	PumpController pumpControl(blackBoard, bus);
//	pumpControl.ready();

//	LampController lampControl(blackBoard, bus);
//	lampControl.ready();

	BackRestController::registerInterfaces(blackBoard, bus);
	BackWebSocket::registerInterfaces(blackBoard, bus);

	BackWebSocket sock;

//	// Запуск в отдельном потоке
//	drogon::app()
//			.addListener("0.0.0.0", 8848)
//			.setThreadNum(1)
//			.run();


	while(true) {
		int a = 10;
		std::this_thread::sleep_for(std::chrono::milliseconds{100});
	}
}
