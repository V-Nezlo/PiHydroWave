#pragma once

#include <UtilitaryRS/RsTypes.hpp>
#include <array>
#include <cstdint>

enum class PumpModes : uint8_t {
	EBBNormal,
	EBBSwing,
	Dripping,
};

namespace Log {

enum class Level : unsigned { ERROR = 0, WARN = 1, INFO = 2, DEBUG = 3, TRACE = 4 };

}
