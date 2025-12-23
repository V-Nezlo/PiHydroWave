#pragma once

#include <chrono>

class TimeWrapper {
public:
	static std::chrono::microseconds microseconds()
	{
		auto now = std::chrono::steady_clock::now();
		return std::chrono::duration_cast<std::chrono::microseconds>(
			now.time_since_epoch());
	}

	static std::chrono::milliseconds milliseconds()
	{
		auto now = std::chrono::steady_clock::now();
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			now.time_since_epoch());
	}

	static std::chrono::seconds seconds()
	{
		auto now = std::chrono::steady_clock::now();
		return std::chrono::duration_cast<std::chrono::seconds>(
			now.time_since_epoch());
	}
};
