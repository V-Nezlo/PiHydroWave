#pragma once

#include <string>

namespace Names {

/*
 * Система имен проекта
 * Работаем так:
 * 1) Имя устройства, например pump
 * 2) Тип записи, config,telem,int и rs
 * 3) Имя записи, например Status, value, или имя конфига
 * Например:
 * pump.telem.state
 * pump.config.onTime
 * pump.int.desiredState
 *
 * lamp.telem.Status
 * lamp.telem.flags
 */

// Имена устройств
static constexpr std::string kMultiControllerDev = "multiController";
static constexpr std::string kWaterLevelDev      = "waterLevel";
static constexpr std::string kPPMMeterDev        = "ppmMeter";
static constexpr std::string kPumpDev            = "pump";
static constexpr std::string kLampDev            = "lamp";
static constexpr std::string kUpperLevelDev      = "upperLevel";
static constexpr std::string kPHMeterDev         = "phMeter";
static constexpr std::string kTurbidimeterDev    = "turbidimeter";
static constexpr std::string kTemperatureDev     = "temperature";
static constexpr std::string kBridgeDev          = "bridge";
static constexpr std::string kSystemDev          = "system";
// Типы записей
static constexpr std::string kTelemPostfix   = ".telem";
static constexpr std::string kConfigPostfix  = ".config";
static constexpr std::string kIntPostfix     = ".int";
static constexpr std::string kRsPostfix      = ".rs";
// Записи
static constexpr std::string kValueEnder     = ".value";
static constexpr std::string kStatusEnder    = ".status";
static constexpr std::string kStatusStrEnder = ".statusStr";
// Предсоставленные имена подписок
static const std::string kTelemPipe          = kMultiControllerDev + kRsPostfix + ".data";
static const std::string kTelemBridgeStatus  = kBridgeDev + kRsPostfix + ".status";
// Параметры
static const std::string kLampEnabled        = kLampDev + kConfigPostfix + ".enabled"; // bool
static const std::string kLampOnTime         = kLampDev + kConfigPostfix + ".onTime"; // secs
static const std::string kLampOffTime        = kLampDev + kConfigPostfix + ".offTime"; // secs

static const std::string kPumpEnabled        = kPumpDev + kConfigPostfix + ".enabled"; // bool
static const std::string kPumpMode           = kPumpDev + kConfigPostfix + ".mode"; // PumpModes (Swing..)
static const std::string kPumpOnTime         = kPumpDev + kConfigPostfix + ".onTime"; // secs
static const std::string kPumpOffTime        = kPumpDev + kConfigPostfix + ".offTime"; // sec
static const std::string kPumpSwingTime      = kPumpDev + kConfigPostfix + ".swingTime"; // sec
static const std::string kPumpValidTime      = kPumpDev + kConfigPostfix + ".validTime"; // sec
static const std::string kPumpMaxFloodTime   = kPumpDev + kConfigPostfix + ".maxFloodTime"; // sec

static const std::string kBridgeMacs         = kBridgeDev + kConfigPostfix + ".mac"; // Список со строками
static const std::string kSystemMaintance    = kSystemDev + kConfigPostfix + ".maintance"; // bool
static const std::string kWaterLevelMinLevel = kWaterLevelDev + kConfigPostfix + ".minValue"; // значение от 0 до 100
// Внутренние переменные для работы
static const std::string kPumpPlainType      = kPumpDev + kIntPostfix + ".plainType"; // Осушение-орошение
static const std::string kPumpNextSwitchTime = kPumpDev + kIntPostfix + ".nextSwitchTime"; // время до переключения
static const std::string kPumpDesiredState   = kPumpDev + kIntPostfix + ".desiredState"; // Желаемое состояние насоса
static const std::string kPumpSwingState     = kPumpDev + kIntPostfix + ".swingState"; // состояние качелей

static inline std::string getValueNameByDevice(const std::string &aDeviceName)
{
	return aDeviceName + Names::kTelemPostfix + Names::kValueEnder;
}

static inline std::string getStatusNameByDevice(const std::string &aDeviceName)
{
	return aDeviceName + Names::kTelemPostfix + Names::kStatusEnder;
}

static inline std::string getStatusStrByDevice(const std::string &aDeviceName)
{
	return aDeviceName + Names::kTelemPostfix + Names::kStatusStrEnder;
}

}
