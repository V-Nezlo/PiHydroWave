#pragma once

#include <any>
#include <mutex>
#include <iostream>
#include <queue>
#include <string>
#include <string_view>
#include <unordered_map>
#include <functional>
#include <optional>
#include <vector>
#include <thread>

enum class EventType {
	Log,
	PumpSetState,
	LampSetState,
};

class EventBusObserver {
public:
	virtual void handleEvent(EventType aEv, std::any &aValue) = 0;
};

class EventBus {
public:
	void sendEvent(EventType aEv, std::any aValue)
	{
		queue.push(std::make_pair(aEv, aValue));
	}

	void start()
	{
		thread = std::thread(&EventBus::threadFunction, this);
		thread.detach();
	}

private:
	std::thread thread;

	std::vector<EventBusObserver *> observers;
	std::queue<std::pair<EventType, std::any>> queue;

	void threadFunction()
	{
		if (!queue.empty()) {
			auto event = queue.front();
			queue.pop();

			for (auto &pos : observers) {
				pos->handleEvent(event.first, event.second);
			}
		}
	}
};
