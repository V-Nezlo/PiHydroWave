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
#include <core/Blackboard.hpp>
#include <core/BlackboardEntry.hpp>
#include <core/EventBus.hpp>
#include <memory>
#include <thread>

class LampController {
public:
	LampController(std::shared_ptr<Blackboard> aBb, std::shared_ptr<EventBus> aEvBus);
	bool ready() const;
	void process();
	void start();

private:
	std::shared_ptr<Blackboard> bb;
	std::shared_ptr<EventBus> bus;

	BlackboardEntry<bool> enabled;
	BlackboardEntry<bool> state;
	BlackboardEntry<uint32_t> onTime;
	BlackboardEntry<uint32_t> offTime;
	BlackboardEntry<bool> maintance;
	MonitorEntry monitor;

	std::chrono::milliseconds lastCheckTime;
	std::mutex mutex;
	std::thread thread;

	void sendCommand(bool aNewLampState);
	bool isTimeForLampActive();
};

#endif // INCLUDE_LIGHTCONTROLLER_HPP_
