#pragma once

#include "logger/Logger.hpp"
#include <any>
#include <chrono>
#include <core/Blackboard.hpp>
#include <core/EventBus.hpp>
#include <core/Helpers.hpp>
#include <core/Types.hpp>

#include <nlohmann/json.hpp>
#include <drogon/WebSocketConnection.h>
#include <drogon/WebSocketController.h>

#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>

class BackWebSocket : public drogon::WebSocketController<BackWebSocket, false>, public AbstractPrefixObserver {
public:
	BackWebSocket() = default;

	// AbstractEntryObserver interface
	void onPrefixUpdated(std::string_view /*prefix*/, std::string_view entry, const std::any &value) override
	{
		nlohmann::json msg{{"type", "telemetry"}, {"key", entry}, {"value", toJson(value)}};
		auto str = msg.dump();
		broadcastTelemetry(msg.dump());
	}

	void handleNewConnection(const drogon::HttpRequestPtr &req, const drogon::WebSocketConnectionPtr &conn) override
	{
		auto path = req->path();
		std::lock_guard lock(mutex);

		if (path == "/ws/telemetry") {
			telemetryClients.insert(conn);
			sendInitialTelemetry(conn);
		} else if (path == "/ws/logs") {
			logClients.insert(conn);
		}

		HYDRO_LOG_STATUS("New websocket connection on path: " + path);
	}

	void handleConnectionClosed(const drogon::WebSocketConnectionPtr &conn) override
	{
		std::lock_guard lock(mutex);
		telemetryClients.erase(conn);
		logClients.erase(conn);

		HYDRO_LOG_STATUS("Web socket closed");
	}

	void handleNewMessage(const drogon::WebSocketConnectionPtr &conn, std::string &&message,
		const drogon::WebSocketMessageType &type) override
	{
		(void)message;
		(void)conn;
		(void)type;
	}

	void registerInterfaces(std::shared_ptr<Blackboard> aBb, std::shared_ptr<EventBus> aBus)
	{
		bb = aBb;
		bus = aBus;

		bb->subscribeToPrefix(".telem", this);
		bb->subscribeToPrefix(".int", this);
		bb->subscribeToPrefix(".config", this);
	}

	void log(Log::Level aLevel, std::string &aMsg)
	{
		std::string message = Helpers::getLogLevelName(aLevel);
		message += ": ";
		message += aMsg;

		nlohmann::json msg{{"type", "logs"}, {"key", "log"}, {"value", message}};
		broadcastLogs(message);
	}

	WS_PATH_LIST_BEGIN
	WS_PATH_ADD("/ws/telemetry", drogon::Get);
	WS_PATH_ADD("/ws/logs", drogon::Get);
	WS_PATH_LIST_END

private:
	std::shared_ptr<Blackboard> bb;
	std::shared_ptr<EventBus> bus;

	std::unordered_set<drogon::WebSocketConnectionPtr> telemetryClients;
	std::unordered_set<drogon::WebSocketConnectionPtr> logClients;

	std::recursive_mutex mutex;

	void sendInitialTelemetry(const drogon::WebSocketConnectionPtr &conn)
	{
		const std::vector<std::string> telemKeys = bb->getKeysByPrefix(".telem");
		for (const auto &key : telemKeys) {
			nlohmann::json msg{{"type", "telemetry"}, {"key", key}, {"value", toJson(bb->getAny(key).value())}};
			conn->send(msg.dump());
		}

		const std::vector<std::string> configKeys = bb->getKeysByPrefix(".config");
		for (const auto &key : configKeys) {
			nlohmann::json msg{{"type", "telemetry"}, {"key", key}, {"value", toJson(bb->getAny(key).value())}};
			conn->send(msg.dump());
		}

		const std::vector<std::string> intKeys = bb->getKeysByPrefix(".int");
		for (const auto &key : intKeys) {
			nlohmann::json msg{{"type", "telemetry"}, {"key", key}, {"value", toJson(bb->getAny(key).value())}};
			conn->send(msg.dump());
		}
	}

	void broadcastTelemetry(const std::string &msg)
	{
		std::lock_guard lock(mutex);

		for (auto &c : telemetryClients)
			if (c->connected())
				c->send(msg);
	}

	void broadcastLogs(const std::string &msg)
	{
		std::lock_guard lock(mutex);

		for (auto &c : logClients)
			if (c->connected())
				c->send(msg);
	}

	static nlohmann::json toJson(const std::any &v)
	{
		if (v.type() == typeid(int))
			return std::any_cast<int>(v);
		if (v.type() == typeid(double))
			return std::any_cast<double>(v);
		if (v.type() == typeid(float))
			return std::any_cast<float>(v);
		if (v.type() == typeid(bool))
			return std::any_cast<bool>(v);
		if (v.type() == typeid(std::string))
			return std::any_cast<std::string>(v);
		if (v.type() == typeid(uint64_t))
			return std::any_cast<uint64_t>(v);
		if (v.type() == typeid(DeviceStatus))
			return std::any_cast<int>(v);
		if (v.type() == typeid(unsigned)) {
			const unsigned value = std::any_cast<unsigned>(v);
			return value;
		}
		if (v.type() == typeid(std::chrono::seconds)) {
			const auto seconds = std::any_cast<std::chrono::seconds>(v);
			return static_cast<unsigned>(seconds.count());
		}

		return "unsupported";
	}
};
