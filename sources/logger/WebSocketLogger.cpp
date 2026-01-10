#include <logger/WebSocketLogger.hpp>

namespace Log {

std::shared_ptr<BackWebSocket> WebSocketLogger::socket{};
Level WebSocketLogger::level{Level::ERROR};

void WebSocketLogger::registerSocket(std::shared_ptr<BackWebSocket> aSocket)
{
	socket = aSocket;
}

void WebSocketLogger::log(Level aLevel, std::string &msg)
{
	if (level < aLevel ) {
		return;
	}

	socket->log(aLevel, msg);
}

void WebSocketLogger::setLevel(Level aLevel)
{
	level = aLevel;
}

} // namespace Log
