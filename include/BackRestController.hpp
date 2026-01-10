#pragma once

#include "logger/Logger.hpp"
#include <chrono>
#include <core/Blackboard.hpp>
#include <core/EventBus.hpp>

#include "json/value.h"
#include <drogon/HttpController.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpTypes.h>
#include <drogon/RequestStream.h>
#include <drogon/WebSocketController.h>
#include <nlohmann/json.hpp>

#include <any>
#include <memory>
#include <sstream>
#include <storage/Database.hpp>
#include <string>

using json = nlohmann::json;

class BackRestController : public drogon::HttpController<BackRestController, false> {
public:
	METHOD_LIST_BEGIN
	ADD_METHOD_TO(BackRestController::getValue, "/entry", drogon::Get);
	ADD_METHOD_TO(BackRestController::setValue, "/entry", drogon::Put);
	ADD_METHOD_TO(BackRestController::getHistory, "/history", drogon::Get);
	METHOD_LIST_END

	void getValue(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback)
	{
		using namespace drogon;

		const auto params = req->getParameters();
		const std::string query = params.count("q") ? params.at("q") : std::string{};
		Json::Value result(Json::objectValue);

		if (!query.empty()) {
			std::istringstream iss(query);
			std::string key;

			while (std::getline(iss, key, '+')) {
				if (!bb->has(key)) {
					HYDRO_LOG_ERROR("Rest: getValue: BB has not key: " + key);
					result[key] = Json::nullValue;
					continue;
				}

				if (bb->isType<bool>(key)) {
					if (auto v = bb->get<bool>(key)) result[key] = v.value();

				} else if (bb->isType<unsigned>(key)) {
					if (auto v = bb->get<unsigned>(key)) result[key] = v.value();

				} else if (bb->isType<int>(key)) {
					if (auto v = bb->get<int>(key)) {
						result[key] = v.value();
						int i = v.value();
						int a = v.value();
					}

				} else if (bb->isType<float>(key)) {
					if (auto v = bb->get<unsigned>(key)) result[key] = v.value();

				} else if (bb->isType<std::string>(key)) {
					if (auto v = bb->get<std::string>(key)) result[key] = v.value();

				} else if (bb->isType<std::chrono::milliseconds>(key)) {
					if (auto v = bb->get<std::chrono::milliseconds>(key)) result[key] = static_cast<unsigned>(v.value().count());

				} else if (bb->isType<std::chrono::seconds>(key)) {
					if (auto v = bb->get<std::chrono::seconds>(key)) result[key] = static_cast<unsigned>(v.value().count());

				} else {
					result[key] = bb->getTypeName(key);
					// result[key] = static_cast<int>(1);
				}

			}
		}

		auto resp = HttpResponse::newHttpJsonResponse(result);
		callback(resp);
	}

	void setValue(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback)
	{
		using namespace drogon;

		const std::string bodyStr = std::string(req->getBody());
		Json::Value input;
		Json::CharReaderBuilder builder;
		std::string errs;
		std::istringstream bodyStream(bodyStr);

		if (!Json::parseFromStream(builder, bodyStream, &input, &errs)) {
			auto resp = HttpResponse::newHttpResponse();
			resp->setStatusCode(k400BadRequest);
			resp->setBody(std::string("Invalid JSON: ") + errs);
			callback(resp);
			return;
		}

		if (!input.isObject()) {
			auto resp = HttpResponse::newHttpResponse();
			resp->setStatusCode(k400BadRequest);
			resp->setBody("Expected JSON object");
			callback(resp);
			return;
		}

		for (const auto &name : input.getMemberNames()) {
			const Json::Value &val = input[name];

			if (bb->has(name)) {

				// Будем писать в BB по тому типу что даст BB
				if (bb->isType<bool>(name)) {
					bb->set<bool>(name, val.asBool());

				} else if (bb->isType<unsigned>(name)) {
					bb->set<unsigned>(name, val.asUInt());

				} else if (bb->isType<int>(name)) {
					bb->set<int>(name, val.asInt());

				} else if (bb->isType<float>(name)) {
					bb->set<float>(name, val.asFloat());

				} else if (bb->isType<std::string>(name)) {
					bb->set<std::string>(name, val.asString());

				} else if (bb->isType<std::chrono::seconds>(name)) {
					bb->set<std::chrono::seconds>(name, std::chrono::seconds{val.asUInt()});

				} else if (bb->isType<std::chrono::milliseconds>(name)) {
					bb->set<std::chrono::milliseconds>(name, std::chrono::milliseconds{val.asUInt()});

				} else {
					HYDRO_LOG_ERROR("Rest: setValue : unsupported type");
				}
			}
		}

		auto resp = HttpResponse::newHttpResponse();
		resp->setStatusCode(k204NoContent);
		callback(resp);
	}

	void registerInterfaces(std::shared_ptr<Blackboard> aBb, std::shared_ptr<EventBus> aBus)
	{
		bb = aBb;
		bus = aBus;
	}

	void getHistory(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback)
	{
		using namespace drogon;

		const auto params = req->getParameters();
		std::string key = params.count("key") ? params.at("key") : "";
		std::string fromTs = params.count("from") ? params.at("from") : "1970-01-01 00:00:00";
		std::string toTs = params.count("to") ? params.at("to") : "9999-12-31 23:59:59";
		size_t limit = params.count("limit") ? std::stoul(params.at("limit")) : 1000;

		if (key.empty()) {
			auto resp = HttpResponse::newHttpResponse();
			resp->setStatusCode(k400BadRequest);
			resp->setBody("Missing ?key=");
			callback(resp);
			return;
		}

		// Вытащить данные из DB
		auto items = db->queryHistory(key, fromTs, toTs, limit);
		Json::Value out(Json::arrayValue);

		for (auto &[k, type, value, ts] : items) {
			Json::Value item;
			item["key"] = k;
			item["type"] = type;
			item["value"] = value;
			item["timestamp"] = ts;
			out.append(item);
		}

		auto resp = HttpResponse::newHttpJsonResponse(out);
		callback(resp);
	}

private:
	std::shared_ptr<Blackboard> bb;
	std::shared_ptr<EventBus> bus;
	std::shared_ptr<Database> db;
};
