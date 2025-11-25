
#include "ArgParser.hpp"
#include "BackRestController.hpp"
#include "BackWebSocket.hpp"
#include "DrogonApp.hpp"
#include "LampController.hpp"
#include "PumpController.hpp"
#include "RadioHandler.hpp"
#include "SerialEspProxy.hpp"
#include "core/Blackboard.hpp"
#include "core/EventBus.hpp"
#include "core/Logger.hpp"
#include "core/TimeWrapper.hpp"
#include "packages/ConfigPackage.hpp"
#include "packages/DatabasePackage.hpp"
#include "storage/Database.hpp"

#include <UtilitaryRS/Crc64.hpp>
#include <UtilitaryRS/Crc8.hpp>
#include <UtilitaryRS/DeviceHub.hpp>
#include <UtilitaryRS/RsTypes.hpp>

#include <drogon/HttpAppFramework.h>
#include <drogon/orm/DbClient.h>

#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <thread>

using Hub = RS::DeviceHub<10, SerialEspProxy, TimeWrapper, Crc8, Crc64, 256>;

int main(int argc, char *argv[])
{
	auto args = parseArgs(argc, argv);
	Log::Logger::setLevel(args.logLevel);

	auto bb = std::make_shared<Blackboard>();
	auto bus = std::make_shared<EventBus>();

	if (args.dbPath) {
		auto db = std::make_shared<Database>(args.dbPath.value());
		DatabasePackage dbPackage(bb, db);
	}

	DrogonApp drogonApp;
	drogonApp.start();

	ConfigPackage config(args.configPath, bb);
	if (!config.load()) {
		std::cout << "Config not found, creating..." << std::endl;
	}

	RS::DeviceVersion ver;
	RadioHandler<Hub> radioHandler{args.interfacePath, ver, bb, bus};
	radioHandler.start();

	PumpController pumpControl(bb, bus);
	LampController lampControl(bb, bus);

	BackRestController::registerInterfaces(bb, bus);
	BackWebSocket::registerInterfaces(bb, bus);

	BackWebSocket sock;

	while (true) {
		if (!pumpControl.isStarted() && pumpControl.ready()) {
			pumpControl.start();
		}
		if (!lampControl.isStarted() && lampControl.ready()) {
			lampControl.start();
		}

		std::this_thread::sleep_for(std::chrono::seconds{1});
	}
}
