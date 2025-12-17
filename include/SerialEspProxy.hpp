#pragma once

#include "core/Blackboard.hpp"
#include "core/BlackboardEntry.hpp"
#include "core/Helpers.hpp"
#include "core/InterfaceList.hpp"
#include "core/Types.hpp"

//#include <EspNowUSBProto/Parser.hpp>
#include "../lib/EspNowProto/include/EspNowUSBProto/Parser.hpp"

#include <UtilitaryRS/Crc8.hpp>
#include <UtilitaryRS/RsParser.hpp>
#include <UtilitaryRS/RsTypes.hpp>

#include <array>
#include <chrono>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>

class SerialEspProxy : public AbstractSerial {
	using Parser = RS::RsParser<64, Crc8>;

	// clang-format off
	enum class BridgeStatus {
		Lost    = 0,
		Ready   = 1,
		Working = 2
	};
	// clang-format on

	AbstractSerial *driver;
	std::shared_ptr<Blackboard> bb;
	EspNowBinaryParser espParser;
	std::queue<std::vector<uint8_t>> inbox;

	std::chrono::time_point<std::chrono::steady_clock> lastHeartbeatTime;
	std::chrono::time_point<std::chrono::steady_clock> lastMessageTimepoint;

	std::thread thread;
	BlackboardEntry<BridgeStatus> bridgeStatus;
	std::unordered_map<std::string, std::vector<uint8_t>> uidMap;

public:
	SerialEspProxy(AbstractSerial *aDriver, std::shared_ptr<Blackboard> aBb) :
		driver{aDriver},
		bb{aBb},
		espParser{},
		inbox{},
		lastHeartbeatTime{std::chrono::milliseconds{0}},
		lastMessageTimepoint{std::chrono::milliseconds{0}},
		thread{},
		bridgeStatus{"bridge.status", bb},
		uidMap{}
	{
		bridgeStatus.set(BridgeStatus::Lost);
	}

	void start()
	{
		thread = std::thread(&SerialEspProxy::threadFunction, this);
		thread.detach();
	}

	/// \brief Враппер для EspNow, оборачивает сообщение в контейнер с MAC
	/// \param aData данные для отправки
	/// \param aLength длина данных для отправки
	/// \return
	size_t write(const uint8_t *aData, size_t aLength) override
	{
		const uint8_t uid = Parser::getReceiverFromMsg(aData, aLength);
		const auto keys = bb->getKeysByPrefix("slaves.mac.");
		std::array<uint8_t, 6> macArray;

		if (uid == RS::kReservedUID) {
			// Широковещательный UID, нужно отправить всем доступным MAC адресам
			for (const auto &key : keys) {
				if (const auto macStr = bb->get<std::string>(key)) {
					if (Helpers::unpackMac(macStr.value(), macArray)) {
						uint8_t message[250];
						const size_t len = espParser.assemblePacket(macArray, aData, aLength, message, sizeof(message));

						if (len) {
							driver->write(message, len);
						} else {
							return 0;
						}
					}
				}
			}
		} else {
			// Common сообщение, отправляем требуемому адресату
			for (const auto &key : keys) {
				if (const auto macStr = bb->get<std::string>(key)) {
					// Если есть соответствующая запись в таблице MAC->UID
					if (uidMap.contains(macStr.value())) {
						// Проверим какие UID принадлежат этому адресу
						auto val = uidMap.at(macStr.value());
						auto it = std::find(val.begin(), val.end(), uid);
						// Если нашли нужный - отправляем
						if (it != val.end()) {
							if (Helpers::unpackMac(macStr.value(), macArray)) {
								uint8_t message[250];
								const size_t len
									= espParser.assemblePacket(macArray, aData, aLength, message, sizeof(message));

								if (len) {
									driver->write(message, len);
									break;
								} else {
									return 0;
								}
							}
						}
					}
				}
			}
		}

		return 0;
	}

	/// \brief Получение сырых данных из UART
	/// \param data
	/// \param len
	void feed(const uint8_t *data, size_t len)
	{
		size_t left = len;

		while (left) {
			size_t parsed = espParser.update(data + (len - left), left);

			if (parsed == 0) {
				espParser.reset();
				break;
			}

			left -= parsed;

			if (espParser.state() == EspNowBinaryParser::State::Done) {
				// Вот тут уже должно быть собранное сообщение UtilitaryRS
				// Соотнесем UID и MAC через парсинг хедера
				const uint8_t uid = Parser::getTranceiverFromMsg(espParser.payload(), espParser.payloadSize());
				std::array<uint8_t, 6> newMac;

				// Safety
				if (!espParser.getMac(newMac)) {
					return;
				}

				// 0xFF MAC это пинг от бриджа, нет смысла делать что-то с пакетом дальше
				if (newMac[0] == 0xFF && newMac[1] == 0xFF && newMac[2] == 0xFF && newMac[3] == 0xFF
					&& newMac[4] == 0xFF && newMac[5] == 0xFF) {
					std::cout << "Bridge heartbeat received!" << std::endl;
					lastHeartbeatTime = std::chrono::steady_clock::now();
					espParser.reset();
					return;
				}

				const auto keys = bb->getKeysByPrefix("slaves.mac.");
				// Проверять MAC нет смысла, разрулится автоматически
				// Вместо этого посмотрим зареган ли он в таблице MAC-UID
				const auto macStr = Helpers::packMac(newMac);
				const bool haveMac = uidMap.contains(macStr);

				// Если такой MAC уже зарегистрирован - пробежимся по списку UID
				if (haveMac) {
					auto &val = uidMap.at(macStr);
					auto it = std::find(val.begin(), val.end(), uid);
					// Если UID не найден - запихнем новый
					if (it == val.end()) {
						val.push_back(uid);
					}
				} else {
					uidMap[macStr].push_back(uid);
				}

				// Сохраняем полезную нагрузку во внутреннюю очередь
				std::vector<uint8_t> packet(espParser.payload(), espParser.payload() + espParser.payloadSize());
				inbox.push(std::move(packet));

				lastMessageTimepoint = std::chrono::steady_clock::now();
				espParser.reset();
			}
		}
	}

	/// Получить готовый полезный пакет (если есть)
	size_t read(void *aData, size_t aLen) override
	{
		if (inbox.empty()) {
			return 0;
		}

		auto &front = inbox.front();
		size_t len = std::min(aLen, front.size());
		memcpy(aData, front.data(), len);
		inbox.pop();
		return len;
	}

private:
	auto createPingPacket()
	{
		std::array<uint8_t, 9> packet = {0};
		std::array<uint8_t, 6> mac = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
		uint8_t reserved = 0xAA;
		espParser.assemblePacket(mac, &reserved, 1, packet.data(), packet.size());
		return packet;
	}

	void threadFunction()
	{
		while (true) {
			std::chrono::time_point<std::chrono::steady_clock> time = std::chrono::steady_clock::now();

			switch (bridgeStatus.read()) {
				case BridgeStatus::Lost: {
					auto packet = createPingPacket();
					driver->write(packet.data(), packet.size());
					if (lastHeartbeatTime.time_since_epoch().count()
						&& time - lastHeartbeatTime <= std::chrono::seconds{5}) {
						bridgeStatus.set(BridgeStatus::Ready);
					}
				} break;
				case BridgeStatus::Ready: {
					auto packet = createPingPacket();
					driver->write(packet.data(), packet.size());

					if (time - lastMessageTimepoint <= std::chrono::seconds{5}) {
						bridgeStatus.set(BridgeStatus::Working);
					} else if (time - lastHeartbeatTime >= std::chrono::seconds{5}) {
						bridgeStatus.set(BridgeStatus::Lost);
					}
				} break;
				case BridgeStatus::Working:
					if (time - lastMessageTimepoint >= std::chrono::seconds{5}) {
						lastHeartbeatTime = time;
						bridgeStatus.set(BridgeStatus::Ready);
					}
					break;
			}

			std::this_thread::sleep_for(std::chrono::seconds{1});
		}
	}
};
