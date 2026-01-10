#pragma once
#include "SerialEspProxy.hpp"

#include "core/Blackboard.hpp"
#include "core/BlackboardEntry.hpp"
#include "core/EventBus.hpp"
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

	BlackboardEntry<HydroRS::MultiControllerTelem> telemPipe;

	std::thread receive;
	std::thread transmit;

public:
	RadioHandler(std::string aSerial, RS::DeviceVersion &aHubVersion, std::shared_ptr<Blackboard> aBb,
				 std::shared_ptr<EventBus> aEvBus);

	void start();
	void receiveThread();
	void probe();
	void processThread();

	// DeviceHubObserver interface
	void onAckNotReceivedEv(const std::string &aName, RS::MessageType) override;
	void onAckReceivedEv(const std::string &aName, RS::MessageType aMessage, RS::Result aCode) override;
	void onCommandResultEv(const std::string &aName, RS::Result aReturn) override;
	void onRequestErrorEv(const std::string &aName, RS::Result aReturn) override;
	RS::Result blobAnswerEvReceived(const std::string &aName, uint8_t aRequest, const void *aData, size_t aSize) override;
	void deviceRegisteredEv(const std::string &aName, RS::DeviceVersion aVersion) override;
	void deviceLostEv(const std::string &aName) override;
	RS::Result fileWriteResultEv(const std::string &aName, RS::Result aReturn) override;
	void deviceHealthReceivedEv(const std::string &aName, RS::Health aHealth, uint16_t aFlags) override;

	// EventBusObserver interface
	void handleEvent(EventType aEv, std::any &aValue) override;

private:
	void createSchedules();
};
