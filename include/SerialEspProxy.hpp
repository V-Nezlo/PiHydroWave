#pragma once

#include "core/Blackboard.hpp"
#include "core/InterfaceList.hpp"

//#include <EspNowUSBProto/Parser.hpp>
#include "../lib/EspNowProto/include/EspNowUSBProto/Parser.hpp"

#include <UtilitaryRS/Crc8.hpp>
#include <UtilitaryRS/RsParser.hpp>
#include <UtilitaryRS/RsTypes.hpp>

#include <array>
#include <memory>
#include <queue>

class SerialEspProxy : public AbstractSerial {
	using Parser = RS::RsParser<64, Crc8>;

	AbstractSerial *driver;
	std::shared_ptr<Blackboard> bb;
	EspNowBinaryParser espParser;
	std::queue<std::vector<uint8_t>> inbox;

	struct MacSlot {
		uint8_t mac[6];
		uint8_t reserved;
		uint8_t uid;
	};

public:
	SerialEspProxy(AbstractSerial *aDriver, std::shared_ptr<Blackboard> aBb) : driver{aDriver}, bb{aBb}, espParser{}
	{
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
				if (bb->get<uint64_t>(key).has_value()) {
					uint64_t pack = bb->get<uint64_t>(key).value();
					MacSlot slot;
					memcpy(&slot, &pack, sizeof(pack));
					memcpy(macArray.data(), &slot.mac, macArray.size());

					uint8_t message[250];
					const size_t len = espParser.assemblePacket(macArray, aData, aLength, message, sizeof(message));

					if (len) {
						driver->write(message, len);
					} else {
						return 0;
					}
				}
			}
		} else {
			// Common сообщение, отправляем требуемому адресату
			for (const auto &key : keys) {
				if (bb->get<uint64_t>(key).has_value()) {
					uint64_t pack = bb->get<uint64_t>(key).value();
					MacSlot slot;
					memcpy(&slot, &pack, sizeof(pack));

					if (slot.uid == uid) {
						memcpy(macArray.data(), &slot.mac, macArray.size());

						uint8_t message[250];
						const size_t len = espParser.assemblePacket(macArray, aData, aLength, message, sizeof(message));

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
				const uint8_t uid = Parser::getReceiverFromMsg(espParser.payload(), espParser.payloadSize());
				std::array<uint8_t, 6> newMac;
				espParser.getMac(newMac);

				const auto keys = bb->getKeysByPrefix("slaves.mac.");

				for (const auto &key : keys) {
					if (const auto entry = bb->get<uint64_t>(key)) {
						MacSlot slot;
						memcpy(&slot, &entry.value(), sizeof(slot));
						std::array<uint8_t, 6> mac;
						memcpy(mac.data(), &slot.mac, mac.size());

						if (mac == newMac) {
							// Если UID холостой - заполняем, иначе - не трогаем, UtilitaryRS сам разберется от кого
							if (slot.uid == RS::kReservedUID) {
								slot.uid = uid;

								uint64_t packed;
								memcpy(&packed, &slot, sizeof(packed));
								bb->set(key, packed);
							}
						}
					}
				}

				// Сохраняем полезную нагрузку во внутреннюю очередь
				std::vector<uint8_t> packet(espParser.payload(), espParser.payload() + espParser.payloadSize());
				inbox.push(std::move(packet));

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
};
