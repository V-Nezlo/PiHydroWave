#pragma once

#include <UtilitaryRS/RsTypes.hpp>
#include <array>
#include <cstdint>

enum class PumpModes {
	EBBNormal,
	EBBSwing,
	Dripping,
};

// clang-format off
enum class DeviceStatus {
	NotFound    = 0,
	Warning     = 1,
	Working     = 2,
	Error       = 3
};
// clang-format on

namespace Log {

enum class Level {STATUS = 0, ERROR = 1, WARN = 2, INFO = 3, DEBUG = 4, TRACE = 5 };

}
