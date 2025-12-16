#pragma once

#include "SettingsManager.hpp"
#include "core/Blackboard.hpp"
#include "core/Helpers.hpp"
#include <memory>
#include <string>

struct ConfigPackage {
	ConfigPackage(std::string aFileName, std::shared_ptr<Blackboard> aBb) :
		name{aFileName},
		bb{aBb},
		manager{aBb, aFileName}
	{
		registerSettings();
	}

	void registerSettings()
	{
		manager.registerSetting("config.lamp.enabled", SettingType::BOOL, true, "Enable lamp");
		manager.registerSetting("config.lamp.onTime", SettingType::UNSIGNED, 0, "Lamp on time in minutes");
		manager.registerSetting("config.lamp.offTime", SettingType::UNSIGNED, 0, "Lamp off time in minutes");

		manager.registerSetting("config.pump.enabled", SettingType::BOOL, true, "Enable pump");
		manager.registerSetting("config.pump.mode", SettingType::UNSIGNED, 0, "Pump mode");
		manager.registerSetting("config.pump.onTime", SettingType::UNSIGNED, 0, "Pump on time in seconds");
		manager.registerSetting("config.pump.offTime", SettingType::UNSIGNED, 0, "Pump off time in seconds");
		manager.registerSetting("config.pump.swingTime", SettingType::UNSIGNED, 0, "Pump swing time in seconds");
		manager.registerSetting("config.pump.validTime", SettingType::UNSIGNED, 0, "Upper validation time in seconds");
		manager.registerSetting("config.pump.maxFloodTime", SettingType::UNSIGNED, 0, "Max time for tank flooding in secs");
		manager.registerSetting("config.pump.minWaterLevel", SettingType::UNSIGNED, 0, "Minimal water level in percent for pump operation");

		manager.registerSetting("slaves.mac.1", SettingType::STRING, "E8:31:CD:D6:D1:B4", "MAC1");
	}

	bool load()
	{
		return manager.load();
	}

private:
	std::string name;
	std::shared_ptr<Blackboard> bb;

	SettingsManager manager;
};
