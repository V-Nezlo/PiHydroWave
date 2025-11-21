#ifndef BACKEND_HPP
#define BACKEND_HPP

#include "json/value.h"
#include <any>
#include <drogon/HttpResponse.h>
#include <memory>
#include <nlohmann/json.hpp>

#include "core/Blackboard.hpp"
#include "core/EventBus.hpp"
#include <drogon/HttpController.h>
#include <drogon/HttpTypes.h>
#include <drogon/RequestStream.h>
#include <drogon/WebSocketController.h>
#include <sstream>
#include <storage/Database.hpp>
#include <string>

using json = nlohmann::json;

class BackRestController : public drogon::HttpController<BackRestController> {
public:
	METHOD_LIST_BEGIN
	ADD_METHOD_TO(BackRestController::getValue, "/entry", drogon::Get);
	ADD_METHOD_TO(BackRestController::setValue, "/entry", drogon::Put);
	ADD_METHOD_TO(BackRestController::getHistory, "/history", drogon::Get);
	METHOD_LIST_END

	void getValue(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr&)>&& callback)
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
					result[key] = Json::nullValue;
					continue;
				}

				if (bb->isType<bool>(key)) {
					if (auto v = bb->get<bool>(key)) result[key] = *v;
					else result[key] = Json::nullValue;
				} else if (bb->isType<int>(key)) {
					if (auto v = bb->get<int>(key)) result[key] = *v;
					else result[key] = Json::nullValue;
				} else if (bb->isType<uint64_t>(key)) {
					if (auto v = bb->get<uint64_t>(key)) result[key] = *v;
					else result[key] = Json::nullValue;
				} else if (bb->isType<double>(key)) {
					if (auto v = bb->get<double>(key)) result[key] = *v;
					else result[key] = Json::nullValue;
				} else if (bb->isType<std::string>(key)) {
					if (auto v = bb->get<std::string>(key)) result[key] = *v;
					else result[key] = Json::nullValue;
				} else {
					// Неизвестный/сложный тип — возвращаем строковое представление типа
					result[key] = bb->getTypeName(key);
				}
			}
		}

		auto resp = HttpResponse::newHttpJsonResponse(result);
		callback(resp);
	}

	void setValue(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr&)>&& callback)
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

			if (val.isBool()) {
				bb->set<bool>(name, val.asBool());

			} else if (val.isInt()) {
				bb->set<int>(name, val.asInt());

			} else if (val.isDouble()) {
				bb->set<double>(name, val.asDouble());

			} else if (val.isUInt64()) {
				bb->set<uint64_t>(name, val.asUInt64());
			}
		}

		auto resp = HttpResponse::newHttpResponse();
		resp->setStatusCode(k204NoContent);
		callback(resp);
	}

	static void registerInterfaces(std::shared_ptr<Blackboard> aBb, std::shared_ptr<EventBus> aBus)
	{
		bb = aBb;
		bus = aBus;
	}

	void getHistory(const drogon::HttpRequestPtr &req,
					std::function<void(const drogon::HttpResponsePtr &)> &&callback)
	{
		using namespace drogon;

		const auto params = req->getParameters();
		std::string key     = params.count("key") ? params.at("key") : "";
		std::string fromTs  = params.count("from") ? params.at("from") : "1970-01-01 00:00:00";
		std::string toTs    = params.count("to")   ? params.at("to")   : "9999-12-31 23:59:59";
		size_t limit        = params.count("limit") ? std::stoul(params.at("limit")) : 1000;

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
	static std::shared_ptr<Blackboard> bb;
	static std::shared_ptr<EventBus> bus;
	static std::shared_ptr<Database> db;
};

std::shared_ptr<Blackboard> BackRestController::bb;
std::shared_ptr<EventBus> BackRestController::bus;
std::shared_ptr<Database> BackRestController::db;

#endif // BACKEND_HPP
