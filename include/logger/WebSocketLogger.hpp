#pragma once

#include "BackWebSocket.hpp"
#include "core/Types.hpp"
#include <memory>

namespace Log {

class WebSocketLogger {
	static std::shared_ptr<BackWebSocket> socket;
	static Level level;

public:
	static void registerSocket(std::shared_ptr<BackWebSocket> aSocket);
	static void log(Log::Level aLevel, std::string &msg);
	static void setLevel(Level aLevel);
};

} // namespace Log
