/*!
@file
@brief Обработчик лампы
@author V-Nezlo (vlladimirka@gmail.com)
@date 09.04.2024
@version 1.0
*/

#include "LampController.hpp"
#include <any>
#include <chrono>
#include <ctime>
#include <thread>

using namespace std::chrono;

LampController::LampController(std::shared_ptr<Blackboard> aBb, std::shared_ptr<EventBus> aEvBus) :
	bb{aBb},
	bus{aEvBus},

	enabled{"config.lamp.enabled", aBb},
	state{"lamp.state", aBb},
	onTime{"config.lamp.onTime", aBb},
	offTime{"config.lamp.offTime", aBb},
	maintance{"config.maintance", aBb},
	monitor{aBb},

	lastCheckTime{0},
	mutex{}
{
}

bool LampController::ready() const
{
	const auto enabledB = enabled.present();
	const auto stateB = state.present();
	const auto onTimeB = onTime.present();
	const auto offTimeB = offTime.present();
	const auto maintanceB = maintance.present();

	if (enabledB && stateB && onTimeB && offTimeB && maintanceB) {
		bus->sendEvent(EventType::Log, std::string("Lamp Controller ready!"));
		return true;
	} else {
		bus->sendEvent(EventType::Log, std::string("Lamp Controller not ready!"));
		return false;
	}
}

void LampController::start()
{
	thread = std::thread(&LampController::process, this);
	thread.detach();
}

void LampController::process()
{
	while(true) {
		const bool desiredLampState = isTimeForLampActive();
		const bool currentState = state.read();

		if (currentState != desiredLampState) {
			sendCommand(desiredLampState);
		}

		std::this_thread::sleep_for(std::chrono::seconds{30});
	}
}

void LampController::sendCommand(bool aNewLampState)
{
	bus->sendEvent(EventType::LampSetState, aNewLampState);
}

bool LampController::isTimeForLampActive()
{
	using namespace std::chrono;

	auto now = system_clock::now();
	time_t tt = system_clock::to_time_t(now);
	tm local{};
	local = *std::localtime(&tt);

	uint32_t minutes = static_cast<uint32_t>(local.tm_hour * 60 + local.tm_min);

	if (onTime.read() <= offTime.read()) {
		return minutes >= onTime.read() && minutes < offTime.read();
	} else {
		return minutes >= onTime.read() || minutes < offTime.read();
	}
}

