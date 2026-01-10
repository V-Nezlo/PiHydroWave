#pragma once

#include "BbNames.hpp"
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
		manager.registerSetting(Names::kLampEnabled, SettingType::BOOL, true, "Enable lamp");
		manager.registerSetting(Names::kLampOnTime, SettingType::INT, 0, "Lamp on time in minutes");
		manager.registerSetting(Names::kLampOffTime, SettingType::INT, 0, "Lamp off time in minutes");

		manager.registerSetting(Names::kPumpEnabled, SettingType::BOOL, true, "Enable pump");
		manager.registerSetting(Names::kPumpMode, SettingType::INT, 0, "Pump mode");

		manager.registerSetting(Names::kPumpOnTime, SettingType::SECONDS, 15, "Pump on time in seconds");
		manager.registerSetting(Names::kPumpOffTime, SettingType::SECONDS, 30, "Pump off time in seconds");
		manager.registerSetting(Names::kPumpSwingTime, SettingType::SECONDS, 8, "Pump swing time in seconds");
		manager.registerSetting(Names::kPumpValidTime, SettingType::SECONDS, 50, "Upper validation time in seconds");
		manager.registerSetting(Names::kPumpMaxFloodTime, SettingType::SECONDS, 180, "Max time for tank flooding in secs");

		manager.registerSetting(Names::kWaterLevelMinLevel, SettingType::FLOAT, 0.f, "Minimal water level in percent for pump operation");

		manager.registerSetting(Names::kSystemMaintance, SettingType::BOOL, false, "System maintance mode");
		manager.registerSetting(Names::kBridgeMacs + ".1", SettingType::STRING, "E8:31:CD:D6:D1:B4", "MAC1");
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
