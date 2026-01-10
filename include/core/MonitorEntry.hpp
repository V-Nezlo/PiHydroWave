#pragma once

#include <core/Blackboard.hpp>
#include <memory>

// clang-format off
enum class MonitorFlags {
	PumpNotOperate         = 1 << 0,
	NotFloodedInTime       = 1 << 1,
	FloatLevelTimeout      = 1 << 2,
	PumpControllerLost     = 1 << 3,
	NoUpperForSwing        = 1 << 4,
	LampControllerLost     = 1 << 5
};
// clang-format on

class MonitorEntry {
	static constexpr std::string kMonName = "DeviceFlags";
public:
	MonitorEntry(std::shared_ptr<Blackboard> aBb) : bb{aBb}
	{
	}

	void setFlag(MonitorFlags aFlag)
	{
		const uint32_t flags = bb->get<uint32_t>(kMonName).value();
		const uint32_t newFlag = static_cast<uint32_t>(aFlag);
		const uint32_t newFlags = flags | newFlag;
		bb->set(kMonName, newFlags);
	}

	void clearFlag(MonitorFlags aFlag)
	{
		const uint32_t flags = bb->get<uint32_t>(kMonName).value();
		const uint32_t newFlag = static_cast<uint32_t>(aFlag);
		const uint32_t newFlags = flags & ~newFlag;
		bb->set(kMonName, newFlags);
	}

	bool isFlagSet(MonitorFlags aFlag)
	{
		const uint32_t flags = bb->get<uint32_t>(kMonName).value();
		return flags & static_cast<uint32_t>(aFlag);
	}

	std::string_view getName() const
	{
		return kMonName;
	}

	void subscribe(AbstractEntryObserver *aObs)
	{
		return bb->subscribe(kMonName, aObs);
	}

	void invoke()
	{
		uint32_t defaultValue = 0;
		bb->set<uint32_t>(kMonName, std::move(defaultValue));
	}

private:
	std::shared_ptr<Blackboard> bb;

	bool present() const
	{
		return bb->has(kMonName);
	}
};
