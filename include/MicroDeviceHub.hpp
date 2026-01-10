/*!
@file
@brief Обработчик микродевайсов - подписывается на трубу телеметрии и распихивает по всем микродевайсам
@author V-Nezlo (vlladimirka@gmail.com)
@date 09.04.2024
@version 1.0
*/

#pragma once

#include "core/Helpers.hpp"
#include "BbNames.hpp"
#include "core/Blackboard.hpp"
#include "core/BlackboardEntry.hpp"
#include "core/MonitorEntry.hpp"
#include "core/RadioTypes.hpp"
#include "core/Types.hpp"
#include "uDevice.hpp"
#include <memory>
#include <string>

class MicroDeviceHub : public AbstractEntryObserver {
	std::shared_ptr<Blackboard> bb;

	BlackboardEntry<DeviceStatus> bridgeStatus;
	BlackboardEntry<HydroRS::MultiControllerTelem> telemPipe;
	MonitorEntry monitor;

	enum DeviceType {Pump, Lamp, WaterLevel, PPMMeter, PHMeter, Turbidimeter, Temperature, UpperLevel, Bridge, System};
	std::unordered_map<DeviceType, UDevice> list;

public:
	MicroDeviceHub(std::shared_ptr<Blackboard> aBb):
		bridgeStatus{Names::kTelemBridgeStatus, aBb},
		telemPipe{Names::kTelemPipe, aBb},
		monitor{aBb}
	{
		list.emplace(Pump, UDevice{Names::kPumpDev, aBb});
		list.emplace(Lamp, UDevice{Names::kLampDev, aBb});
		list.emplace(WaterLevel, UDevice{Names::kWaterLevelDev, aBb});
		list.emplace(PPMMeter, UDevice{Names::kPPMMeterDev, aBb});
		list.emplace(PHMeter, UDevice{Names::kPHMeterDev, aBb});
		list.emplace(Turbidimeter, UDevice{Names::kTurbidimeterDev, aBb});
		list.emplace(Bridge, UDevice{Names::kBridgeDev, aBb});
		list.emplace(Temperature, UDevice{Names::kTemperatureDev, aBb});
		list.emplace(UpperLevel, UDevice{Names::kUpperLevelDev, aBb});
		list.emplace(System, UDevice{Names::kSystemDev, aBb});

		bridgeStatus.subscribe(this);
		telemPipe.subscribe(this);
		monitor.subscribe(this);
	}

	// AbstractEntryObserver interface
	void onEntryUpdated(std::string_view aEntry, const std::any &aValue)
	{
		if (aEntry == bridgeStatus.getName()) {
			parseBridgeStatus(aValue);
		} else if (aEntry == telemPipe.getName()) {
			parseTelemPipe(aValue);
		} else if (aEntry == monitor.getName()) {
			parseSystemFlags(aValue);
		}
	}

private:
	void parseBridgeStatus(const std::any & /*aValue*/)
	{
		const auto status = bridgeStatus();
		std::string str;

		switch(status) {
			case DeviceStatus::Error:
				str += "Bridge not answering";
				break;
			case DeviceStatus::NotFound:
				str += "Bridge serial device lost";
				break;
			case DeviceStatus::Warning:
				str += "Bridge working but no slaves founded";
				break;
			case DeviceStatus::Working:
				str += "Bridge and slaves working";
				break;
		}

		list.at(Bridge).updateValue<bool>(true);
		list.at(Bridge).updateStatus(std::any_cast<DeviceStatus>(status));
		list.at(Bridge).updateStatusStr(str);
	}

	void parseTelemPipe(const std::any &aValue)
	{
		HydroRS::MultiControllerTelem telem = std::any_cast<HydroRS::MultiControllerTelem>(aValue);

		// Тут выведу типы руками, поскольку видел как автоматически разрешаемые типы разрешились неверно
		// Хотя в случае использования фундаментальных типов такого не должно произойти, никаких u8-u16 тут нет
		// Потом перепроверю
		list.at(Pump).updateValue<bool>(telem.pumpState);
		list.at(Lamp).updateValue<bool>(telem.lampState);
		list.at(WaterLevel).updateValue<float>(telem.waterLevel);
		list.at(PHMeter).updateValue<float>(telem.ph);
		list.at(PPMMeter).updateValue<float>(telem.ppm);
		list.at(Turbidimeter).updateValue<float>(telem.turbidimeter);
		list.at(UpperLevel).updateValue<bool>(telem.upperState);
		list.at(Temperature).updateValue<float>(telem.temperature);
		list.at(System).updateValue<bool>(true);

		for (auto &dev : list) {
			// Систему и бридж пропускаем, они работают по другому
			if (dev.first == DeviceType::System || dev.first == DeviceType::Bridge) {
				continue;
			}

			const auto flags = getDeviceFlags(dev.first, telem);
			const auto tuple = getUDeviceStatus(dev.first, flags);

			list.at(dev.first).updateStatus(std::get<0>(tuple));
			list.at(dev.first).updateStatusStr(std::get<1>(tuple));
		}
	}

	void parseSystemFlags(const std::any & /*aValue*/)
	{
		// Проще вытянуть флаги из самого монитора чем их парсить
		bool working = true;
		DeviceStatus status = DeviceStatus::Working;
		std::string str = "SystemStatus:\n";

		if (monitor.isFlagSet(MonitorFlags::FloatLevelTimeout)) {
			status = DeviceStatus::Warning;
			str += "Float level stucked\n";
		}

		if (monitor.isFlagSet(MonitorFlags::NotFloodedInTime)) {
			status = DeviceStatus::Warning;
			str += "Upper tank not flooded in time\n";
		}

		if (monitor.isFlagSet(MonitorFlags::PumpNotOperate)) {
			str += "Pump control disabled by error\n";
			status = DeviceStatus::Error;
			working = false;
		}

		if (monitor.isFlagSet(MonitorFlags::NoUpperForSwing)) {
			str += "Pump mode setted as SWING but Upper not present\n";
			str += "PumpController wait for upper";
			working = false;
		}

		if (monitor.isFlagSet(MonitorFlags::PumpControllerLost)) {
			status = DeviceStatus::Error;
			str += "PumpController can't operate - lost\n";
			str += "PumpControllers stopped!\n";
			working = false;
		}

		if (monitor.isFlagSet(MonitorFlags::LampControllerLost)) {
			status = DeviceStatus::Error;
			str += "LampController can't operate - lost\n";
			str += "LampControllers stopped!\n";
			working = false;
		}

		if (str == "SystemStatus:\n") {
			str += "No errors";
		}

		list.at(System).updateValue(working);
		list.at(System).updateStatus(status);
		list.at(System).updateStatusStr(str);
	}

	static constexpr uint8_t getDeviceFlags(DeviceType aType, HydroRS::MultiControllerTelem &aTelem)
	{
		using namespace Helpers;
		using namespace HydroRS;

		switch (aType) {
			case DeviceType::Pump:
				return asU8(aTelem.pumpStatus);
			case DeviceType::Lamp:
				return asU8(aTelem.lampStatus);
			case DeviceType::WaterLevel:
				return asU8(aTelem.waterLevelStatus);
			case DeviceType::UpperLevel:
				return asU8(aTelem.upperStatus);
			case DeviceType::PPMMeter:
				return asU8(aTelem.ppmStatus);
			case DeviceType::PHMeter:
				return asU8(aTelem.phStatus);
			case DeviceType::Temperature:
				return asU8(aTelem.temperatureStatus);
			case DeviceType::Turbidimeter:
				return asU8(aTelem.turbidimeterStatus);
			default:
				return 0;
		}
	}

	static constexpr std::tuple<DeviceStatus, std::string> getUDeviceStatus(DeviceType aType, uint8_t aFlags)
	{
		using namespace Helpers;
		using namespace HydroRS;

		DeviceStatus status = DeviceStatus::Working;
		std::string str;

		switch (aType) {
			case DeviceType::Pump:
				if (aFlags & asU8(PumpStatus::PumpNotPresent)) {
					status = DeviceStatus::NotFound;
					str += "Pump not present\n";
				}
				if (aFlags & asU8(PumpStatus::PumpOvercurrent)) {
					status = DeviceStatus::Error;
					str += "Pump overcurrent!\n";
				}
				break;
			case DeviceType::Lamp:
				if (aFlags & asU8(LampStatus::AcNotPresent)) {
					status = DeviceStatus::Error;
					str += "AC voltage for LAMP not present\n";
				}
				break;
			case DeviceType::WaterLevel:
				if (aFlags & asU8(WaterLevelStatus::SensorNotFound)) {
					status = DeviceStatus::NotFound;
					str += "NO SENSOR!\n";
				}
				if (aFlags & asU8(WaterLevelStatus::NoWater)) {
					status = DeviceStatus::Error;
					str += "NO WATER!\n";
				}
				if (aFlags & asU8(WaterLevelStatus::SensorError)) {
					status = DeviceStatus::Error;
					str += "Water sensor error!\n";
				}
				break;
			case DeviceType::PPMMeter:
				if (aFlags & asU8(PPMStatus::PPMSensorNotFound)) {
					status = DeviceStatus::NotFound;
					str += "No PPM meter found\n";
				}
				if (aFlags & asU8(PPMStatus::SystemDefective)) {
					status = DeviceStatus::Warning;
					str += "System incomplete for calculating PPM\n";
				}
				if (aFlags & asU8(PPMStatus::PPMSensorIncorrectVal)) {
					status = DeviceStatus::Warning;
					str += "PPM sensor wrong value\n";
				}
				break;
			case DeviceType::PHMeter:
				if (aFlags & asU8(PHStatus::PHSensorNotFound)) {
					status = DeviceStatus::NotFound;
					str += "PH sensor not found";
				}
				break;
			case DeviceType::Temperature:
				if (aFlags & asU8(TemperatureStatus::TempSensorNotFound)) {
					status = DeviceStatus::NotFound;
					str += "Temperature sensor not found\n";
				}
				if (aFlags & asU8(TemperatureStatus::TempSensorError)) {
					status = DeviceStatus::Error;
					str += "Temperature sensor error\n";
				}
				if (aFlags & asU8(TemperatureStatus::TempSensorWrongData)) {
					status = DeviceStatus::Warning;
					str += "Temperature sensor wrong data\n";
				}
				break;
			case DeviceType::UpperLevel:
				if (aFlags & asU8(UpperStatus::UpperNotFound)) {
					status = DeviceStatus::NotFound;
					str += "UpperLevel not found";
				}
				break;
			case DeviceType::Turbidimeter:
				if (aFlags & asU8(TurbidimeterStatus::TurbidimeterNotFound)) {
					status = DeviceStatus::NotFound;
					str += "Turbidimeter not found\n";
				}
				if (aFlags & asU8(TurbidimeterStatus::TurbidimeterError)) {
					status = DeviceStatus::Error;
					str += "Turbidimeter error\n";
				}
				break;
			default:
				break;
		}

		if (str.empty()) {
			str = "Works fine";
		}

		return std::make_tuple(status, str);
	}
};
