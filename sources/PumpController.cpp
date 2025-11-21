/*!
@file
@brief Обработчик насоса
@author V-Nezlo (vlladimirka@gmail.com)
@date 09.04.2024
@version 1.0
*/

#include "PumpController.hpp"
#include "core/EventBus.hpp"
#include "core/MonitorEntry.hpp"
#include "core/Types.hpp"
#include <any>
#include <chrono>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

PumpController::PumpController(std::shared_ptr<Blackboard> aBb, std::shared_ptr<EventBus> aEvBus) :
	bb{aBb},
	bus{aEvBus},

	monitor{aBb},

	enabled{"config.pump.enabled", aBb},
	present{"pump.present", aBb},
	mode{"config.pump.mode", aBb},
	state{"pump.state", aBb},
	desiredState{"pump.desiredState", aBb},
	pumpOnTime{"config.pump.onTime", aBb},
	pumpOffTime{"config.pump.offTime", aBb},
	upperState{"pump.upperState", aBb},
	swingTime{"config.pump.swingTime", aBb},
	validTime{"config.pump.validTime", aBb},
	maxFloodTime{"config.pump.maxFloodTime", aBb},
	plainType{"pump.plainType", aBb},
	swingState{"pump.swingState", aBb},
	maintance{"config.maintance", aBb},
	waterLevel{"pump.waterLevel", aBb},
	minWaterLevel{"config.pump.minWaterLevel", aBb},

	lastActionTime{0},
	lastSwingTime{0},
	waterFillingTimer{0},
	lastChecksTime{0},
	lastValidatorTime{0},
	fillingCheckEn{false}
{

}

bool PumpController::ready() const
{
	const bool enabledB = enabled.present();
	const bool presentB = present.present();
	const bool modeB = mode.present();
	const bool stateB = state.present();
	const bool desiredStateB = desiredState.present();
	const bool pumpOnTimeB = pumpOnTime.present();
	const bool pumpOffTimeB = pumpOffTime.present();
	const bool upperStateB = upperState.present();
	const bool swingTimeB = swingTime.present();
	const bool validTimeB = validTime.present();
	const bool maxFloodTimeB = maxFloodTime.present();
	const bool plainTypeB = plainType.present();
	const bool swingStateB = swingState.present();
	const bool maintanceB = maintance.present();

	// Провалидирую все требуемые настройки
	if (enabledB && presentB && modeB && stateB && desiredStateB && pumpOnTimeB && pumpOffTimeB && upperStateB && swingTimeB && validTimeB && maxFloodTimeB && plainTypeB && swingStateB && maintanceB) {
		bus->sendEvent(EventType::Log, std::string("Pump Controller ready!"));
		return true;
	} else {
		bus->sendEvent(EventType::Log, std::string("Pump Controller not ready!"));
		return false;
	}
}

void PumpController::start()
{
	bus->sendEvent(EventType::Log, std::string("Pump Controller started!"));
	mode.subscribe(this);
	thread = std::thread(&PumpController::process, this);
	thread.detach();
}

void PumpController::onEntryUpdated(std::string_view entry, const std::any &value)
{
	if (entry == mode.getName()) {
		std::lock_guard lock(mutex);
		updateMode(std::any_cast<PumpModes>(value));
	}
}

void PumpController::process()
{
	while (true) {
		auto now = std::chrono::steady_clock::now();
		std::chrono::milliseconds time = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());

		if (maintance.read()) {
			std::this_thread::sleep_for(std::chrono::milliseconds{1000});
			continue;
		}

		std::lock_guard lock(mutex);
		// Если ловера нет - то и управлять нечем
		if (present.read()) {
			switch (mode.read()) {
				case PumpModes::EBBNormal:
					processEBBNormalMode(time);
					break;
				case PumpModes::EBBSwing:
					processEBBSwingMode(time);
					break;
				case PumpModes::Dripping:
					processDripMode(time);
					break;
				default:
					break;
			}
			// Обработка требуемого состояния насоса
			if (time > lastValidatorTime + validTime.read()) {
				lastValidatorTime = time;

				if (state.read() != desiredState.read()) {
					bus->sendEvent(EventType::PumpSetState, desiredState.read());
				}
			}
			// Если ловера нет но мы пытаемся им управлять - ставим ошибку
		} else if (enabled.read() && !present.read()) {
			monitor.setFlag(MonitorFlags::ControllerLost);
		} else {
			if (desiredState.read()) {
				bus->sendEvent(EventType::PumpSetState, false);
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds{200});
	}
}

void PumpController::updateMode(PumpModes)
{
	lastActionTime = milliseconds{0};
	lastSwingTime = milliseconds{0};
	waterFillingTimer = milliseconds{0};
	plainType.set(PlainType::Drainage);
	fillingCheckEn = false;
}

bool PumpController::permitForAction() const
{
	return present.read() && waterLevel.read() > minWaterLevel.read();
}

/// @brief EBB режим, вкл выкл насоса по времени
void PumpController::processEBBNormalMode(std::chrono::milliseconds aCurrentTime)
{
	switch (plainType.read()) {
		case PlainType::Irrigation:
		// Если насос сейчас включен - смотрим, не пора ли выключать
			if (aCurrentTime > lastActionTime + pumpOnTime.read()) {
				lastActionTime = aCurrentTime;
				desiredState = false;
				plainType = PlainType::Drainage;
			}
			break;
		case PlainType::Drainage:
			if (aCurrentTime > lastActionTime + pumpOffTime.read()) {
				lastActionTime = aCurrentTime;
				if (permitForAction()) {
					desiredState = true;
					plainType = PlainType::Irrigation;
					monitor.clearFlag(MonitorFlags::PumpNotOperate);
				} else {
					monitor.setFlag(MonitorFlags::PumpNotOperate);
				}
			}
			break;
	}
}

/// @brief Расширенный EBB режим, вкл выкл насоса по времени и отработка "качелей"
void PumpController::processEBBSwingMode(std::chrono::milliseconds aCurrentTime)
{
	switch (plainType.read()) {
		case PlainType::Irrigation:
			// Если насос сейчас включен - смотрим, не пора ли выключать
			if (aCurrentTime > lastActionTime + pumpOnTime.read()) {
				lastActionTime = aCurrentTime;
				desiredState = false;
				plainType = PlainType::Drainage;
				fillingCheckEn = false;
			} else {
				// Проверим состояние водички во время состояния ирригации
				if (!permitForAction()) {
					desiredState = false;
					plainType = PlainType::Drainage;
					monitor.setFlag(MonitorFlags::PumpNotOperate);
					return;
				}
				if (fillingCheckEn && aCurrentTime > waterFillingTimer) {
					desiredState = false;
					plainType = PlainType::Drainage;
					monitor.setFlag(MonitorFlags::NotFloodedInTime);
					fillingCheckEn = false;
					return;
				}

				// Обработка свинга
				if (swingState.read() == SwingState::SwingOff
					&& aCurrentTime > lastSwingTime + swingTime.read()) {
					desiredState = true;
					swingState = SwingState::SwingOn;
				} else if (swingState.read() == SwingState::SwingOn && upperState.read()) {
					desiredState = false;
					swingState = SwingState::SwingOff;
					lastSwingTime = aCurrentTime;
					monitor.setFlag(MonitorFlags::NotFloodedInTime);
					fillingCheckEn = false;
				}
			}
			break;

		case PlainType::Drainage:
			if (aCurrentTime > lastActionTime + pumpOffTime.read()) {
				lastActionTime = aCurrentTime;
				if (permitForAction()) {
					desiredState = true;
					plainType = PlainType::Irrigation;
					monitor.clearFlag(MonitorFlags::PumpNotOperate);
					swingState = SwingState::SwingOn;

					// Проверим время заполнения бака один раз после осушения
					waterFillingTimer = aCurrentTime + maxFloodTime.read();
					fillingCheckEn = true;
				} else {
					monitor.setFlag(MonitorFlags::PumpNotOperate);
				}
			}
			break;
	}
}

/// @brief Самый простой режим, просто вкл-выкл насоса по времени
void PumpController::processDripMode(std::chrono::milliseconds aCurrentTime)
{
	switch (plainType.read()) {
		case PlainType::Irrigation:
		// Если насос сейчас включен - смотрим, не пора ли выключать
			if (aCurrentTime > lastActionTime + pumpOnTime.read()) {
				lastActionTime = aCurrentTime;
				desiredState = false;
				plainType = PlainType::Drainage;
			}
			break;
		case PlainType::Drainage:
			if (aCurrentTime > lastActionTime + pumpOffTime.read()) {
				lastActionTime = aCurrentTime;
				if (permitForAction()) {
					desiredState = true;
					plainType = PlainType::Irrigation;
					monitor.clearFlag(MonitorFlags::PumpNotOperate);
				} else {
					monitor.setFlag(MonitorFlags::PumpNotOperate);
				}
			}
			break;
	}
}

