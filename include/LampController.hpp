/*!
@file
@brief Обработчик лампы
@author V-Nezlo (vlladimirka@gmail.com)
@date 09.04.2024
@version 1.0
*/

#ifndef INCLUDE_LIGHTCONTROLLER_HPP_
#define INCLUDE_LIGHTCONTROLLER_HPP_

#include "core/MonitorEntry.hpp"
#include "core/Types.hpp"
#include <core/Blackboard.hpp>
#include <core/BlackboardEntry.hpp>
#include <core/EventBus.hpp>
#include <memory>
#include <thread>

class LampController : public AbstractEntryObserver {
public:
	LampController(std::shared_ptr<Blackboard> aBb, std::shared_ptr<EventBus> aEvBus);
	bool ready() const;
	void process();
	void start();
	bool isStarted() const;

	// AbstractEntryObserver interface
	void onEntryUpdated(std::string_view entry, const std::any &value);

private:
	std::shared_ptr<Blackboard> bb;
	std::shared_ptr<EventBus> bus;

	BlackboardEntry<bool> enabled;
	BlackboardEntry<bool> state;
	BlackboardEntry<int> onTime;
	BlackboardEntry<int> offTime;
	BlackboardEntry<bool> maintance;
	BlackboardEntry<DeviceStatus> status;
	MonitorEntry monitor;

	std::chrono::milliseconds lastCheckTime;
	std::mutex mutex;
	std::thread thread;
	bool started;

	void sendCommand(bool aNewLampState);
	bool isTimeForLampActive();
};

#endif // INCLUDE_LIGHTCONTROLLER_HPP_
