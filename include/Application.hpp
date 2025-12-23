#pragma once

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
#include "core/Types.hpp"
#include "logger/DBLogger.hpp"
#include "logger/Logger.hpp"
#include "core/TimeWrapper.hpp"
#include "logger/WebSocketLogger.hpp"
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

class Application {
	Args args;

	std::shared_ptr<Blackboard> bb;
	std::shared_ptr<EventBus> bus;
	std::shared_ptr<Database> db;
	std::unique_ptr<DatabasePackage> dbPackage;

	ConfigPackage config;
	RadioHandler radioHandler;
	PumpController pumpControl;
	LampController lampControl;

	std::shared_ptr<BackWebSocket> sock;
	std::shared_ptr<BackRestController> rest;
	DrogonApp drogonApp;

public:
	Application(Args &&aArgs, RS::DeviceVersion aVersion) :
		args{std::move(aArgs)},
		bb{std::make_shared<Blackboard>()},
		bus{std::make_shared<EventBus>()},
		db{args.dbPath ? std::make_shared<Database>(args.dbPath.value()) : nullptr},
		dbPackage{db ? std::make_unique<DatabasePackage>(bb, db) : nullptr},

		config{args.configPath, bb},
		radioHandler{args.interfacePath, aVersion, bb, bus},

		pumpControl{bb, bus},
		lampControl{bb, bus},

		sock{std::make_shared<BackWebSocket>()},
		rest{std::make_shared<BackRestController>()},
		drogonApp{rest, sock}
	{
		Log::WebSocketLogger::registerSocket(sock);
		Log::DBLogger::registerDB(db);
		Log::Logger::setLevel(static_cast<Log::Level>(args.logLevel));

		if (!config.load()) {
			std::cout << "Config not found, creating..." << std::endl;
		}

		rest->registerInterfaces(bb, bus);
		sock->registerInterfaces(bb, bus);
		rest->initPathRouting();
	}

	int run()
	{
		radioHandler.start();
		drogonApp.start();

		std::this_thread::sleep_for(std::chrono::milliseconds{250});
		radioHandler.probe();

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
};
