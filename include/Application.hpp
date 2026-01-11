#pragma once

#include "ArgParser.hpp"
#include "BackRestController.hpp"
#include "BackWebSocket.hpp"
#include "BbNames.hpp"
#include "DrogonApp.hpp"
#include "LampController.hpp"
#include "PumpController.hpp"
#include "RadioHandler.hpp"
#include "core/Blackboard.hpp"
#include "core/EventBus.hpp"
#include "core/MonitorEntry.hpp"
#include "core/RadioTypes.hpp"
#include "core/Types.hpp"
#include "logger/DBLogger.hpp"
#include "logger/Logger.hpp"
#include "logger/WebSocketLogger.hpp"
#include "packages/ConfigPackage.hpp"
#include "packages/DatabasePackage.hpp"
#include "storage/Database.hpp"
#include "MicroDeviceHub.hpp"

#include <UtilitaryRS/Crc64.hpp>
#include <UtilitaryRS/Crc8.hpp>
#include <UtilitaryRS/DeviceHub.hpp>
#include <UtilitaryRS/RsTypes.hpp>

#include <drogon/HttpAppFramework.h>
#include <drogon/orm/DbClient.h>

#include <chrono>
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
	MicroDeviceHub uDevices;

	std::shared_ptr<BackWebSocket> sock;
	std::shared_ptr<BackRestController> rest;
	DrogonApp drogonApp;

	MonitorEntry monitor;

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

		uDevices{bb},

		sock{std::make_shared<BackWebSocket>()},
		rest{std::make_shared<BackRestController>()},
		drogonApp{rest, sock},

		monitor{bb}
	{
		Log::WebSocketLogger::registerSocket(sock);
		Log::DBLogger::registerDB(db);
		Log::Logger::setLevel(static_cast<Log::Level>(args.logLevel));

		if (!config.load()) {
			std::cout << "Config not found, creating..." << std::endl;
		}

		if (bb->isType<int>(Names::kWaterLevelMinLevel)) {
			std::cout << "catcha!";
		}

		rest->registerInterfaces(bb, bus);
		sock->registerInterfaces(bb, bus);
		rest->initPathRouting();
	}

	int run()
	{
		radioHandler.start();
		drogonApp.start();
		monitor.invoke();
		testPacket();

		std::this_thread::sleep_for(std::chrono::milliseconds{250});
		radioHandler.probe();

		while (true) {
			if (!pumpControl.isStarted() && pumpControl.ready()) {
				pumpControl.start();
			}

			std::this_thread::sleep_for(std::chrono::seconds{1});

			if (!lampControl.isStarted() && lampControl.ready()) {
				lampControl.start();
			}

			std::this_thread::sleep_for(std::chrono::seconds{5});
		}
	}

	void testPacket()
	{
		BlackboardEntry<HydroRS::MultiControllerTelem> telemPipe{Names::kTelemPipe, bb};
		HydroRS::MultiControllerTelem telem;
		telem.pumpState = true;
		telem.lampState = false;
		telem.upperState = false;
		telem.temperature = 13.f;
		telem.ph = 7.5f;
		telem.ppm = 700;
		telem.waterLevel = 70;
		telem.turbidimeter = 100;

		telem.pumpStatus = static_cast<HydroRS::PumpStatus>(0);
		telem.lampStatus = static_cast<HydroRS::LampStatus>(HydroRS::LampStatus::AcNotPresent);
		telem.waterLevelStatus = static_cast<HydroRS::WaterLevelStatus>(0);
		telem.ppmStatus = static_cast<HydroRS::PPMStatus>(0);
		telem.phStatus = static_cast<HydroRS::PHStatus>(0);
		telem.upperStatus = static_cast<HydroRS::UpperStatus>(0);
		telem.temperatureStatus = static_cast<HydroRS::TemperatureStatus>(0);
		telem.turbidimeterStatus = static_cast<HydroRS::TurbidimeterStatus>(0);

		telemPipe.set(telem);
	}

};
