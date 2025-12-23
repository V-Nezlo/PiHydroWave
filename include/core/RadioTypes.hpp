#pragma once

#include <cstdint>

enum class Requests {
	RequestTelemetry = 1
};

enum class Commands {
	SetState
};

struct PumpTelemetry {
	bool pumpState;
	uint8_t waterLevel;
};

struct LampTelemetry {
	bool lampState;
};

