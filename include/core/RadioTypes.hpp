#pragma once

#include <cstdint>

namespace HydroRS {

enum class Requests {
	RequestTelemetry = 1
};

enum class Commands : uint8_t {
	SetPumpState,
	SetLampState,
	Calibrate
};

enum class PumpStatus : uint8_t {
	PumpNotPresent = 1 << 0,
	PumpOvercurrent = 1 << 1
};

enum class LampStatus : uint8_t {
	AcNotPresent = 1 << 0
};

enum class UpperStatus : uint8_t {
	UpperNotFound = 1 << 0,
};

enum class WaterLevelStatus : uint8_t {
	SensorNotFound = 1 << 0,
	NoWater = 1 << 1,
	SensorError = 1 << 2,
};

enum class PPMStatus : uint8_t {
	PPMSensorNotFound = 1 << 0,
	SystemDefective = 1 << 1,
	PPMSensorIncorrectVal = 1 << 2
};

enum class TemperatureStatus : uint8_t {
	TempSensorNotFound = 1 << 0,
	TempSensorWrongData = 1 << 1,
	TempSensorError = 1 << 2
};

enum class PHStatus : uint8_t {
	PHSensorNotFound = 1 << 0,
	PHSensorWrongValue = 1 << 1,
	PHSensorError      = 1 << 2
};

enum class TurbidimeterStatus : uint8_t {
	TurbidimeterNotFound = 1 << 0,
	TurbidimeterError = 1 << 1
};

struct MultiControllerTelem {
	bool pumpState;
	bool lampState;
	bool upperState;

	float waterLevel;
	float ppm;
	float temperature;
	float ph;
	float turbidimeter;

	PumpStatus pumpStatus;
	LampStatus lampStatus;
	UpperStatus upperStatus;
	WaterLevelStatus waterLevelStatus;
	PPMStatus ppmStatus;
	TemperatureStatus temperatureStatus;
	PHStatus phStatus;
	TurbidimeterStatus turbidimeterStatus;

	float pumpCurrent;
} __attribute__((packed));



} // namespace HydroRS
