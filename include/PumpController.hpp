/*!
@file
@brief Обработчик насоса
@author V-Nezlo (vlladimirka@gmail.com)
@date 09.04.2024
@version 1.0
*/

#ifndef INCLUDE_PUMPCONTROLLER_HPP_
#define INCLUDE_PUMPCONTROLLER_HPP_

#include "core/BlackboardEntry.hpp"
#include "core/Blackboard.hpp"
#include "core/MonitorEntry.hpp"
#include "core/EventBus.hpp"
#include "core/Types.hpp"

#include <chrono>
#include <mutex>
#include <thread>

#include <bits/shared_ptr.h>

class PumpController : public AbstractEntryObserver {
	using milliseconds = std::chrono::milliseconds;
	using seconds = std::chrono::seconds;

	enum class PlainType : uint8_t { Drainage, Irrigation };
	enum class SwingState : uint8_t { SwingOn, SwingOff };

	std::shared_ptr<Blackboard> bb;
	std::shared_ptr<EventBus> bus;

	MonitorEntry monitor;

	BlackboardEntry<bool> enabled;
	BlackboardEntry<bool> present;
	BlackboardEntry<PumpModes> mode;
	BlackboardEntry<bool> state;
	BlackboardEntry<bool> desiredState;
	BlackboardEntry<seconds> pumpOnTime;
	BlackboardEntry<seconds> pumpOffTime;
	BlackboardEntry<bool> upperState;
	BlackboardEntry<seconds> swingTime;
	BlackboardEntry<seconds> validTime;
	BlackboardEntry<seconds> maxFloodTime;
	BlackboardEntry<PlainType> plainType;
	BlackboardEntry<SwingState> swingState;
	BlackboardEntry<bool> maintance;
	BlackboardEntry<uint8_t> waterLevel;
	BlackboardEntry<uint8_t> minWaterLevel;

	std::chrono::milliseconds lastActionTime;
	std::chrono::milliseconds lastSwingTime;
	std::chrono::milliseconds waterFillingTimer;
	std::chrono::milliseconds lastChecksTime;
	std::chrono::milliseconds lastValidatorTime;

	bool fillingCheckEn;
	mutable std::mutex mutex;
	std::thread thread;

public:
	PumpController(std::shared_ptr<Blackboard> aBb, std::shared_ptr<EventBus> aEvBus);
	bool ready() const;
	void process();
	void start();

	// AbstractEntryObserver interface
	void onEntryUpdated(std::string_view entry, const std::any &value) override;
private:
	void updateMode(PumpModes aNewMode);

	/// @brief Функция проверки разрешения работы насоса
	/// @return true если включение насоса разрешено
	bool permitForAction() const;

	/// @brief EBB режим, вкл выкл насоса по времени и проверки на флудинг
	void processEBBNormalMode(std::chrono::milliseconds aCurrentTime);

	/// @brief Расширенный EBB режим, вкл выкл насоса по времени и отработка "качелей"
	void processEBBSwingMode(std::chrono::milliseconds aCurrentTime);

	/// @brief Самый простой режим, просто вкл-выкл насоса по времени
	void processDripMode(std::chrono::milliseconds aCurrentTime);
};

#endif // INCLUDE_PUMPCONTROLLER_HPP_
