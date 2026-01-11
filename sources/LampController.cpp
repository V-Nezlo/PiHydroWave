/*!
@file
@brief Обработчик лампы
@author V-Nezlo (vlladimirka@gmail.com)
@date 09.04.2024
@version 1.0
*/

#include "BbNames.hpp"
#include "LampController.hpp"
#include "core/Types.hpp"
#include "logger/Logger.hpp"
#include <any>
#include <chrono>
#include <ctime>
#include <thread>

using namespace std::chrono;

LampController::LampController(std::shared_ptr<Blackboard> aBb, std::shared_ptr<EventBus> aEvBus) :
	bb{aBb},
	bus{aEvBus},

	enabled{Names::kLampEnabled, aBb},
	state{Names::getValueNameByDevice(Names::kLampDev), aBb},
	onTime{Names::kLampOnTime, aBb},
	offTime{Names::kLampOffTime, aBb},
	maintance{Names::kSystemMaintance, aBb},
	status{Names::getStatusNameByDevice(Names::kLampDev), aBb},
	monitor{aBb},

	lastCheckTime{0},
	mutex{},

	started{false}
{
}

bool LampController::ready() const
{
	// Проверки на наличие "живых" подписок
	const auto enabledB = enabled.present();
	const auto statusB = status.present();
	const auto stateB = state.present();
	const auto onTimeB = onTime.present();
	const auto offTimeB = offTime.present();
	const auto maintanceB = maintance.present();

	bool lampFound = false;
	if (statusB) {
		if (status() != DeviceStatus::NotFound) {
			lampFound = true;
		}
	}

	if (enabledB && stateB && onTimeB && offTimeB && maintanceB && statusB && lampFound) {
		HYDRO_LOG_INFO("Lamp Controller ready!");
		return true;
	} else {
		HYDRO_LOG_INFO("Lamp Controller not ready!");
		return false;
	}
}

void LampController::start()
{
	started = true;
	thread = std::thread(&LampController::process, this);
	thread.detach();
}

bool LampController::isStarted() const
{
	return started;
}

void LampController::onEntryUpdated(std::string_view entry, const std::any &)
{
	if (entry == status.getName()) {
		if (status() != DeviceStatus::NotFound) {
			monitor.clearFlag(MonitorFlags::LampControllerLost);
		}
	}
}

void LampController::process()
{
	while(started) {
		const bool desiredLampState = isTimeForLampActive();

		if (maintance()) {
			std::this_thread::sleep_for(std::chrono::seconds{1});
			continue;
		}

		// Выключение потока
		if (status() == DeviceStatus::NotFound) {
			monitor.setFlag(MonitorFlags::LampControllerLost);
			started = false;
			break;
		}

		if (state() != desiredLampState) {
			sendCommand(desiredLampState);
		}

		std::this_thread::sleep_for(std::chrono::seconds{10});
	}

	HYDRO_LOG_ERROR("Lamp controller stopped by error!");
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

	const int minutes = static_cast<int>(local.tm_hour * 60 + local.tm_min);
	const int onTimeMin = onTime();
	const int offTimeMin = offTime();

	if (onTimeMin <= offTimeMin) {
		return minutes >= onTimeMin && minutes < offTimeMin;
	} else {
		return minutes >= onTimeMin || minutes < offTimeMin;
	}
}

