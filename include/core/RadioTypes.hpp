#ifndef RADIOTYPES_HPP
#define RADIOTYPES_HPP

enum class Requests {
	RequestTelemetry = 1
};

enum class Commands {
	SetState
};

#include <cstdint>

struct PumpTelemetry {
	bool pumpState;
	uint8_t waterLevel;
};

struct LampTelemetry {
	bool lampState;
};

#endif // RADIOTYPES_HPP
