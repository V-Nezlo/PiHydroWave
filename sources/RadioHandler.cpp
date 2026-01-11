#include "RadioHandler.hpp"
#include "BbNames.hpp"
#include "logger/Logger.hpp"

using namespace HydroRS;

RadioHandler::RadioHandler(std::string aSerial, RS::DeviceVersion &aHubVersion, std::shared_ptr<Blackboard> aBb, std::shared_ptr<EventBus> aEvBus) :
	bb{aBb},
	bus{aEvBus},
	driver{aSerial, 115200},
	proxy{&driver, bb},
	hub{aHubVersion, proxy},

	telemPipe{Names::kTelemPipe, aBb}
{
	hub.registerObserver(this);
	bus->registerObserver(this);
}

void RadioHandler::start()
{
	proxy.start();

	receive = std::thread(&RadioHandler::receiveThread, this);
	receive.detach();
	transmit = std::thread(&RadioHandler::processThread, this);
	transmit.detach();

	createSchedules();
}

void RadioHandler::receiveThread()
{
	uint8_t buffer[64];

	while (true) {
		if (!driver.poll()) {
			std::this_thread::sleep_for(std::chrono::milliseconds{1000});
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

void RadioHandler::probe()
{
	return hub.probeAll(true, true);
}

void RadioHandler::processThread()
{
	while (true) {
		hub.process(TimeWrapper::milliseconds());
		std::this_thread::sleep_for(std::chrono::milliseconds{50});
	}
}

void RadioHandler::onAckNotReceivedEv(const std::string &aName, RS::MessageType)
{
	HYDRO_LOG_TRACE(aName + " Not answered");
}

void RadioHandler::onAckReceivedEv(const std::string &aName, RS::MessageType aMessage, RS::Result aCode)
{
	HYDRO_LOG_TRACE(aName + " Ack, message: " + std::to_string(static_cast<uint8_t>(aMessage)) + " : "
					+ RS::Helpers::retToString(aCode));
}

void RadioHandler::onCommandResultEv(const std::string &aName, RS::Result aReturn)
{
	HYDRO_LOG_TRACE(aName + " Command returned " + RS::Helpers::retToString(aReturn));
}

void RadioHandler::onRequestErrorEv(const std::string &aName, RS::Result aReturn)
{
	HYDRO_LOG_TRACE(aName + " Request error: " + RS::Helpers::retToString(aReturn));
}

RS::Result RadioHandler::blobAnswerEvReceived(const std::string &aName, uint8_t aRequest, const void *aData, size_t aSize)
{
	if (aName != Names::kMultiControllerDev || aSize != sizeof(MultiControllerTelem)) {
		return RS::Result::Unsupported;
	}

	if (aRequest == static_cast<uint8_t>(Requests::RequestTelemetry)) {
		MultiControllerTelem telem;
		memcpy(&telem, aData, aSize);

		telemPipe = telem;

		return RS::Result::Ok;
	} else {
		return RS::Result::Unsupported;
	}
}

void RadioHandler::deviceRegisteredEv(const std::string &aName, RS::DeviceVersion aVersion)
{
	bb->set(aName + ".rs" + ".present", true);
	bb->set(aName + ".rs" + ".version", aVersion);
}

void RadioHandler::deviceLostEv(const std::string &aName)
{
	bb->set(std::string(aName + ".rs" + ".present"), false);
	HYDRO_LOG_ERROR(aName + "Device lost");
}

RS::Result RadioHandler::fileWriteResultEv(const std::string &aName, RS::Result aReturn)
{
	// Firmware updater
	HYDRO_LOG_INFO(aName + "File write result: " + RS::Helpers::retToString(aReturn));
	return RS::Result::Ok;
}

void RadioHandler::deviceHealthReceivedEv(const std::string &aName, RS::Health aHealth, uint16_t aFlags)
{
	bb->set(aName + ".rs" + ".health", aHealth);
	bb->set(aName + ".rs" + ".flags", aFlags);
}

void RadioHandler::handleEvent(EventType aEv, std::any &aValue)
{
	switch (aEv) {
		case EventType::LampSetState:
			hub.sendCmdToDevice(Names::kMultiControllerDev, static_cast<uint8_t>(Commands::SetLampState), std::any_cast<bool>(aValue));
			break;
		case EventType::PumpSetState:
			hub.sendCmdToDevice(Names::kMultiControllerDev, static_cast<uint8_t>(Commands::SetPumpState), std::any_cast<bool>(aValue));
			break;
		default:
			break;
	}
}

void RadioHandler::createSchedules()
{
	hub.createSchedRequest(Names::kMultiControllerDev, static_cast<uint8_t>(Requests::RequestTelemetry), sizeof(MultiControllerTelem), 500ms);
}
