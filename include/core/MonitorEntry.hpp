#pragma once

#include <core/Blackboard.hpp>
#include <memory>
#include <type_traits>

// clang-format off
enum class MonitorFlags {
	PumpNotOperate         = 1 << 0,
	NotFloodedInTime       = 1 << 1,
	FloatLevelTimeout      = 1 << 2,
	ControllerLost         = 1 << 3,
	PPMSensorError         = 1 << 4,
	PHSensorError          = 1 << 5,
};
// clang-format on

class MonitorEntry {
public:
	MonitorEntry(std::shared_ptr<Blackboard> aBb) : name{"DeviceFlags"}, bb{aBb}
	{
		if (!bb->has(name)) {
			uint32_t defaultValue = 0;
			bb->set(name, defaultValue);
		}
	}

	void setFlag(MonitorFlags aFlag)
	{
		const uint32_t flags = bb->get<uint32_t>(name).value();
		const uint32_t newFlag = static_cast<uint32_t>(aFlag);
		const uint32_t newFlags = flags | newFlag;
		bb->set(name, newFlags);
	}

	void clearFlag(MonitorFlags aFlag)
	{
		const uint32_t flags = bb->get<uint32_t>(name).value();
		const uint32_t newFlag = static_cast<uint32_t>(aFlag);
		const uint32_t newFlags = flags & ~newFlag;
		bb->set(name, newFlags);
	}

	bool isFlagSet(MonitorFlags aFlag)
	{
		const uint32_t flags = bb->get<uint32_t>(name).value();
		return flags & static_cast<uint32_t>(aFlag);
	}

	std::string_view getName() const
	{
		return name;
	}

private:
	const std::string name;
	std::shared_ptr<Blackboard> bb;

	bool present() const
	{
		return bb->has(name);
	}
};
