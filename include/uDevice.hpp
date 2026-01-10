#pragma once

#include "core/Blackboard.hpp"
#include "core/BlackboardEntry.hpp"
#include "BbNames.hpp"
#include "core/Types.hpp"
#include <memory>
#include <string>

class UDevice {
	std::shared_ptr<Blackboard> bb;
	std::string valueName;

	BlackboardEntry<DeviceStatus> status;
	BlackboardEntry<std::string> statusStr;

public:
	UDevice(const std::string &aName, std::shared_ptr<Blackboard> aBb):
		bb{aBb},
		valueName{Names::getValueNameByDevice(aName)},

		status{Names::getStatusNameByDevice(aName), aBb},
		statusStr{Names::getStatusStrByDevice(aName), aBb}
	{

	}

	template<typename T>
	void updateValue(T aValue)
	{
		bb->set<T>(valueName, std::move(aValue));
	}

	void updateStatus(DeviceStatus aStatus)
	{
		status = aStatus;
	}

	void updateStatusStr(const std::string &aStr)
	{
		statusStr = aStr;
	}
};
