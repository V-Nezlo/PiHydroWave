#pragma once
#include "SerialEspProxy.hpp"

#include "core/Blackboard.hpp"
#include "core/BlackboardEntry.hpp"
#include "core/EventBus.hpp"
#include "core/Types.hpp"
#include "logger/Logger.hpp"
#include "core/RadioTypes.hpp"
#include "core/TimeWrapper.hpp"
#include "drivers/SerialDriver.hpp"

#include <UtilitaryRS/Crc64.hpp>
#include <UtilitaryRS/Crc8.hpp>
#include <UtilitaryRS/DeviceHub.hpp>
#include <UtilitaryRS/RsHelpers.hpp>
#include <UtilitaryRS/RsTypes.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

using namespace std::chrono_literals;

class RadioHandler : public RS::DeviceHubObserver, public EventBusObserver {
	using Hub = RS::DeviceHub<10, SerialEspProxy, TimeWrapper, Crc8, Crc64, 256>;

	using milliseconds = std::chrono::milliseconds;
	using seconds = std::chrono::seconds;

	std::shared_ptr<Blackboard> bb;
	std::shared_ptr<EventBus> bus;
	SerialDriver driver;
	SerialEspProxy proxy;

	Hub hub;

	std::thread receive;
	std::thread transmit;

	BlackboardEntry<bool> pumpState;
	BlackboardEntry<bool> upperState;
	BlackboardEntry<bool> lampState;
	BlackboardEntry<uint8_t> waterLevel;

public:
	RadioHandler(std::string aSerial, RS::DeviceVersion &aHubVersion, std::shared_ptr<Blackboard> aBb,
		std::shared_ptr<EventBus> aEvBus) :
		bb{aBb},
		bus{aEvBus},
		driver{aSerial, 115200},
		proxy{&driver, bb},
		hub{aHubVersion, proxy},

		pumpState{"pump.state", aBb},
		upperState{"pump.upperState", aBb},
		lampState{"lamp.state", aBb},
		waterLevel{"pump.waterLevel", aBb}
	{
		hub.registerObserver(this);
		bus->registerObserver(this);
	}

	void start()
	{
		proxy.start();

		receive = std::thread(&RadioHandler::receiveThread, this);
		receive.detach();
		transmit = std::thread(&RadioHandler::processThread, this);
		transmit.detach();

		createSchedules();
	}

	void receiveThread()
	{
		uint8_t buffer[64];

		while (true) {
			if (!driver.poll(50)) {
				continue;
			}

			const size_t len = driver.read(buffer, sizeof(buffer));
			if (!len) {
				continue;
			}

			proxy.feed(buffer, len);
			uint8_t packet[255];

			while (size_t pktLen = proxy.read(packet, sizeof(packet))) {
				hub.update(packet, pktLen);
			}
		}
	}

	void probe()
	{
		return hub.probeAll(true, true);
	}

	void processThread()
	{
		while (true) {
			hub.process(TimeWrapper::milliseconds());
			std::this_thread::sleep_for(std::chrono::milliseconds{50});
		}
	}

	// DeviceHubObserver interface
	void onAckNotReceivedEv(const std::string &aName, RS::MessageType) override
	{
		HYDRO_LOG_TRACE(aName + " Not answered");
	}

	void onAckReceivedEv(const std::string &aName, RS::MessageType aMessage, RS::Result aCode) override
	{
		HYDRO_LOG_TRACE(			aName + " Ack, message: " + std::to_string(static_cast<uint8_t>(aMessage)) + " : "
		+ RS::Helpers::retToString(aCode));
	}

	void onCommandResultEv(const std::string &aName, RS::Result aReturn) override
	{
		HYDRO_LOG_TRACE(aName + " Command returned " + RS::Helpers::retToString(aReturn));
	}

	void onRequestErrorEv(const std::string &aName, RS::Result aReturn) override
	{
		HYDRO_LOG_TRACE(aName + " Request error: " + RS::Helpers::retToString(aReturn));
	}

	RS::Result blobAnswerEvReceived(
		const std::string &aName, uint8_t aRequest, const void *aData, size_t aSize) override
	{
		std::string keyName = aName + std::string(".telem");

		if (aName == "pump" && aRequest == static_cast<uint8_t>(Requests::RequestTelemetry)
			&& aSize == sizeof(PumpTelemetry)) {

			PumpTelemetry telem;
			memcpy(&telem, aData, aSize);

			pumpState.set(telem.pumpState);
			waterLevel.set(telem.waterLevel);

			return RS::Result::Ok;
		} else if (aName == "lamp" && aRequest == static_cast<uint8_t>(Requests::RequestTelemetry)
			&& aSize == sizeof(LampTelemetry)) {

			LampTelemetry telem;
			memcpy(&telem, aData, aSize);

			lampState.set(telem.lampState);

			return RS::Result::Ok;
		} else {
			return RS::Result::Error;
		}
	}

	void deviceRegisteredEv(const std::string &aName, RS::DeviceVersion aVersion) override
	{
		bb->set(aName + ".present", true);
		bb->set(aName + ".version", aVersion);
	}

	void deviceLostEv(const std::string &aName) override
	{
		bb->set(std::string(aName + ".present"), false);
		HYDRO_LOG_ERROR(aName + "Device lost");
	}

	RS::Result fileWriteResultEv(const std::string &aName, RS::Result aReturn) override
	{
		// Firmware updater
		HYDRO_LOG_INFO(aName + "File write result: " + RS::Helpers::retToString(aReturn));
		return RS::Result::Ok;
	}

	void deviceHealthReceivedEv(const std::string &aName, RS::Health aHealth, uint16_t aFlags) override
	{
		bb->set(aName + ".health", aHealth);
		bb->set(aName + ".flags", aFlags);
	}

	// EventBusObserver interface
	void handleEvent(EventType aEv, std::any &aValue) override
	{
		switch (aEv) {
			case EventType::LampSetState:
				hub.sendCmdToDevice("Lamp", static_cast<uint8_t>(Commands::SetState), std::any_cast<bool>(aValue));
				break;
			case EventType::PumpSetState:
				hub.sendCmdToDevice("Pump", static_cast<uint8_t>(Commands::SetState), std::any_cast<bool>(aValue));
				break;
			default:
				break;
		}
	}

private:
	void createSchedules()
	{
		hub.createSchedRequest("pump", static_cast<uint8_t>(Requests::RequestTelemetry), sizeof(PumpTelemetry), 500ms);
		hub.createSchedRequest("lamp", static_cast<uint8_t>(Requests::RequestTelemetry), sizeof(LampTelemetry), 500ms);
	}
};
