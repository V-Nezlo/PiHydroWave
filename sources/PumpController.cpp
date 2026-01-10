/*!
@file
@brief Обработчик насоса
@author V-Nezlo (vlladimirka@gmail.com)
@date 09.04.2024
@version 1.0
*/

#include "PumpController.hpp"
#include "BbNames.hpp"
#include "core/EventBus.hpp"
#include "core/MonitorEntry.hpp"
#include "core/Types.hpp"
#include "logger/Logger.hpp"
#include <any>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>

PumpController::PumpController(std::shared_ptr<Blackboard> aBb, std::shared_ptr<EventBus> aEvBus) :
	bb{aBb},
	bus{aEvBus},
	monitor{aBb},

	enabled{Names::kPumpEnabled, aBb},
	mode{Names::kPumpMode, aBb},
	state{Names::getValueNameByDevice(Names::kPumpDev), aBb},
	desiredState{Names::kPumpDesiredState, aBb},
	pumpOnTime{Names::kPumpOnTime, aBb},
	pumpOffTime{Names::kPumpOffTime, aBb},
	upperState{Names::getValueNameByDevice(Names::kUpperLevelDev), aBb},
	swingTime{Names::kPumpSwingTime, aBb},
	validTime{Names::kPumpValidTime, aBb},
	maxFloodTime{Names::kPumpMaxFloodTime, aBb},
	plainType{Names::kPumpPlainType, aBb},
	swingState{Names::kPumpSwingState, aBb},
	maintance{Names::kSystemMaintance, aBb},
	waterLevel{Names::getValueNameByDevice(Names::kWaterLevelDev), aBb},
	minWaterLevel{Names::kWaterLevelMinLevel, aBb},
	pumpStatus{Names::getStatusNameByDevice(Names::kPumpDev), aBb},
	upperStatus{Names::getStatusNameByDevice(Names::kUpperLevelDev), aBb},
	nextSwitchTime{Names::kPumpNextSwitchTime, aBb},

	lastActionTime{0},
	lastSwingTime{0},
	waterFillingTimer{0},
	lastChecksTime{0},
	lastValidatorTime{0},
	lastTelemetryTime{0},
	fillingCheckEn{false},

	startedFlag{false}
{
	mode.subscribe(this);
	upperStatus.subscribe(this);
	nextSwitchTime = std::chrono::seconds{1};
	swingState = SwingState::SwingOff;
	desiredState = false;
	plainType = PlainType::Drainage;
}

bool PumpController::ready() const
{
	// Проверки на наличие "живых" подписок
	const bool enabledB = enabled.present();
	const bool statusB = pumpStatus.present();
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
	// Доп проверка на состояние насоса
	bool pumpFound = false;
	if (statusB) {
		if (pumpStatus() != DeviceStatus::NotFound) {
			pumpFound = true;
		}
	}

	// Провалидирую все требуемые настройки
	if (enabledB && statusB && modeB && stateB && desiredStateB && pumpOnTimeB && pumpOffTimeB && upperStateB && swingTimeB && validTimeB && maxFloodTimeB && plainTypeB && swingStateB && maintanceB && pumpFound) {
		HYDRO_LOG_INFO("Pump Controller ready!");
		return true;
	} else {
		HYDRO_LOG_INFO("Pump Controller not ready!");
		return false;
	}
}

void PumpController::start()
{
	startedFlag = true;
	thread = std::thread(&PumpController::process, this);
	thread.detach();

	HYDRO_LOG_INFO("Pump Controller started!");
}

bool PumpController::isStarted() const
{
	return startedFlag;
}

void PumpController::onEntryUpdated(std::string_view entry, const std::any &)
{
	if (entry == mode.getName()) {
		std::lock_guard lock(mutex);
		updateMode(mode());
	} else if (entry == upperStatus.getName()) {
		if (upperStatus() != DeviceStatus::NotFound) {
			monitor.clearFlag(MonitorFlags::NoUpperForSwing);
		}
	} else if (entry == pumpStatus.getName()) {
		if (pumpStatus() != DeviceStatus::NotFound) {
			monitor.clearFlag(MonitorFlags::PumpControllerLost);
		}
	}
}

void PumpController::process()
{
	while (startedFlag) {
		auto now = std::chrono::steady_clock::now();
		std::chrono::milliseconds time = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());

		if (maintance()) {
			std::this_thread::sleep_for(std::chrono::milliseconds{1000});
			continue;
		}

		std::lock_guard lock(mutex);

		if (pumpStatus() == DeviceStatus::NotFound) {
			if (enabled()) {
				monitor.setFlag(MonitorFlags::PumpControllerLost);
			}

			std::this_thread::sleep_for(std::chrono::milliseconds{1000});
			startedFlag = false;
			break;
		}

		switch (mode()) {
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

		// Вывод доп информации
		if (time > lastTelemetryTime + std::chrono::milliseconds{200}) {
			lastTelemetryTime = time;

			const milliseconds elapsed = time - lastActionTime;
			const milliseconds actionTime = plainType() == PlainType::Irrigation ? pumpOnTime() : pumpOffTime();
			nextSwitchTime = std::chrono::duration_cast<seconds>(actionTime - elapsed);
		}

		// Обработка требуемого состояния насоса
		if (time > lastValidatorTime + validTime()) {
			lastValidatorTime = time;

			if (state() != desiredState()) {
				bus->sendEvent(EventType::PumpSetState, desiredState());
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds{200});
	}

	HYDRO_LOG_ERROR("Pump controller stopped due an error!");
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
	return waterLevel() > minWaterLevel();
}

/// @brief EBB режим, вкл выкл насоса по времени
void PumpController::processEBBNormalMode(std::chrono::milliseconds aCurrentTime)
{
	switch (plainType()) {
		case PlainType::Irrigation:
		// Если насос сейчас включен - смотрим, не пора ли выключать
			if (aCurrentTime > lastActionTime + pumpOnTime()) {
				lastActionTime = aCurrentTime;
				desiredState = false;
				plainType = PlainType::Drainage;
			}
			break;
		case PlainType::Drainage:
			if (aCurrentTime > lastActionTime + pumpOffTime()) {
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
	switch (plainType()) {
		case PlainType::Irrigation:
			// Если насос сейчас включен - смотрим, не пора ли выключать
			if (aCurrentTime > lastActionTime + pumpOnTime()) {
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
				if (upperStatus() == DeviceStatus::NotFound) {
					desiredState = false;
					plainType = PlainType::Drainage;
					monitor.setFlag(MonitorFlags::NoUpperForSwing);
					fillingCheckEn = false;
					return;
				}

				// Обработка свинга
				if (swingState() == SwingState::SwingOff
					&& aCurrentTime > lastSwingTime + swingTime()) {
					desiredState = true;
					swingState = SwingState::SwingOn;
				} else if (swingState() == SwingState::SwingOn && upperState()) {
					desiredState = false;
					swingState = SwingState::SwingOff;
					lastSwingTime = aCurrentTime;
					monitor.setFlag(MonitorFlags::NotFloodedInTime);
					fillingCheckEn = false;
				}
			}
			break;

		case PlainType::Drainage:
			if (aCurrentTime > lastActionTime + pumpOffTime()) {
				lastActionTime = aCurrentTime;
				if (permitForAction()) {
					desiredState = true;
					plainType = PlainType::Irrigation;
					monitor.clearFlag(MonitorFlags::PumpNotOperate);
					swingState = SwingState::SwingOn;

					// Проверим время заполнения бака один раз после осушения
					waterFillingTimer = aCurrentTime + maxFloodTime();
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
	switch (plainType()) {
		case PlainType::Irrigation:
		// Если насос сейчас включен - смотрим, не пора ли выключать
			if (aCurrentTime > lastActionTime + pumpOnTime()) {
				lastActionTime = aCurrentTime;
				desiredState = false;
				plainType = PlainType::Drainage;
			}
			break;
		case PlainType::Drainage:
			if (aCurrentTime > lastActionTime + pumpOffTime()) {
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

