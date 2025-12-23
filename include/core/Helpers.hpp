#pragma once

#include <core/Types.hpp>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>

namespace Helpers {

static int hexNibble(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
	if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
	return -1;
}

static inline std::string packMac(std::array<uint8_t, 6> aMac)
{
	static const char *hex = "0123456789ABCDEF";
	std::string out(17, '0'); // "XX:XX:XX:XX:XX:XX"

	for (size_t i = 0; i < 6; ++i) {
		const uint8_t b = aMac[i];
		out[i * 3 + 0] = hex[(b >> 4) & 0x0F];
		out[i * 3 + 1] = hex[b & 0x0F];
		if (i != 5) out[i * 3 + 2] = ':';
	}
	return out;
}

// Возвращает true если строка строго в формате "XX:XX:XX:XX:XX:XX"
static inline bool unpackMac(const std::string &aStr, std::array<uint8_t, 6> &out)
{
	if (aStr.size() != 17) return false;

	for (size_t i = 0; i < 6; ++i) {
		const size_t pos = i * 3;

		const int hi = hexNibble(aStr[pos + 0]);
		const int lo = hexNibble(aStr[pos + 1]);
		if (hi < 0 || lo < 0) return false;

		if (i != 5 && aStr[pos + 2] != ':') return false;

		out[i] = static_cast<uint8_t>((hi << 4) | lo);
	}

	return true;
}

static inline std::string getLogLevelName(Log::Level aLevel)
{
	switch(aLevel) {
		case Log::Level::DEBUG:
			return "Debug";
		case Log::Level::ERROR:
			return "Error";
		case Log::Level::INFO:
			return "Info";
		case Log::Level::TRACE:
			return "Trace";
		case Log::Level::WARN:
			return "Warning";
		default:
			return "Unknown";
	}
}


} // namespace Helpers
