/*!
@file
@brief Blackboard
@author V-Nezlo (vlladimirka@gmail.com)
@date 10.09.2025
@version 1.0
*/


#pragma once

#include "core/InterfaceList.hpp"
#include "HeteroLookup.hpp"
#include "core/Types.hpp"
#include "logger/Logger.hpp"

#include <any>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <algorithm>
#include <utility>
#include <vector>
#include <cstddef>
#include <cstring>

class AbstractEntryObserver {
public:
	virtual ~AbstractEntryObserver() = default;
	virtual void onEntryUpdated(std::string_view entry, const std::any &value) = 0;
};

class AbstractPrefixObserver {
public:
	virtual ~AbstractPrefixObserver() = default;
	virtual void onPrefixUpdated(std::string_view prefix, std::string_view entry, const std::any &value) = 0;
};

class Blackboard {
private:
	std::unordered_map<std::string, std::any, StrHash, StrEq> data;
	std::unordered_map<std::string, std::unique_ptr<AbstractValidator>, StrHash, StrEq> validators;

	mutable std::recursive_mutex mutex;

	std::unordered_map<std::string_view, std::vector<AbstractEntryObserver *>> keyObservers;
	std::unordered_map<std::string_view, std::vector<AbstractPrefixObserver *>> prefixObservers;

public:

	/// \brief Установка значения
	/// \param key Ключ
	/// \param value rvalue значения
	/// \return true если поле применено, false если запись не удалась
	template<typename T>
	bool set(std::string_view key, T&& value)
	{
		using V = std::decay_t<T>;

		bool changed = false;

		std::any anyVal = std::any(std::forward<T>(value));
		std::any anyValSnap;
		std::string_view keySnap;

		{
			std::lock_guard lock(mutex);
			auto it = data.find(key);

			// Если ключа нет - аллоцируем строку, кладем внутрь
			if (it == data.end()) {
				auto result = data.try_emplace(std::string(key), anyVal);
				it = result.first;
				changed = true;
			// Если ключ есть - проверим, есть ли смысл вызывать обсерверы
			} else {
				// Но сначала посмотрим список валидаторов
				for (const auto &[entry, validator] : validators) {
					if (key.find(entry) != std::string::npos) {
						if (validator) {
							// Проверяем валидатором запись, если не соответствует - дропаем
							if (!validator->isDataCorrect(anyVal)) {
								return false;
							}
						}
					}
				}

				if (auto* old = std::any_cast<V>(&it->second)) {
					// Если оператор сравнения есть - сравниваем
					if constexpr (requires (const V& a, const V& b) { a == b; }) {
						if (*old != value) {
							it->second = anyVal;
							changed = true;
						}
					// Если нет - считаем измененным
					} else {
						it->second = anyVal;
						changed = true;
					}
				} else {
					HYDRO_LOG_ERROR("Key" + std::string(key) + "trying to change his type, daga kotowaru!");
					return false;
				}
			}

			if (changed) {
				keySnap = it->first;
				anyValSnap = it->second;
			}
		}

		if (changed) {
			notifyObservers(keySnap, anyValSnap);
		}
		return changed;
	}

	/// \brief Получить значение по ключу
	/// \param key Ключ
	/// \return Optional со значением
	template<typename T>
	std::optional<T> get(std::string_view key) const
	{
		std::lock_guard lock(mutex);
		auto it = data.find(key);
		if (it != data.end()) {
			try {
				return std::any_cast<T>(it->second);
			} catch (const std::bad_any_cast &) {
				return std::nullopt;
			}
		}
		std::cerr << "Key " << key << " not found!" << std::endl;
		return std::nullopt;
	}

	/// \brief insertValidator
	/// \param aEntry Может быть и префиксом и именем
	/// \param aValidator ожидается rvalue на валидатор
	/// \return
	bool insertValidator(std::string_view aEntry, std::unique_ptr<AbstractValidator> aValidator)
	{
		if (validators.contains(aEntry)) {
			HYDRO_LOG_ERROR("Validator already registered");
			return false;
		}

		std::string ent = std::string{aEntry};

		const auto result = validators.try_emplace(ent, std::move(aValidator));
		return result.second;
	}

	/// \brief Получить std::any по ключу
	/// \param key Ключ
	/// \return Optional с std::any
	const std::optional<std::any> getAny(std::string_view key)
	{
		std::lock_guard lock(mutex);
		auto it = data.find(key);
		if (it != data.end()) {
			return it->second;
		}

		return std::nullopt;
	}

	/// \brief Получить или дефолтное значение
	/// \param key Ключ
	/// \param defaultValue Значение для возврата при неудачном чтении
	/// \return если чтение успешно - текущее значение, если нет - переданное значение
	template<typename T>
	T getOr(std::string_view key, T defaultValue) const
	{
		return get<T>(key).value_or(defaultValue);
	}

	/// \brief Проверка типа хранимого значения
	/// \param key Ключ
	/// \return true если тип сходится
	template<typename T>
	bool isType(std::string_view key) const
	{
		std::lock_guard lock(mutex);
		auto it = data.find(key);
		if (it != data.end()) {
			return it->second.type() == typeid(T);
		}
		return false;
	}

	/// \brief Проверка наличия записи
	/// \param key Ключ
	/// \return true если запись имеется
	bool has(std::string_view key) const
	{
		std::lock_guard lock(mutex);
		return data.find(key) != data.end();
	}

	/// \brief Удалить запись
	/// \param key Ключ
	/// \return true если удаление успешно
	bool remove(std::string_view key)
	{
		bool existed = false;
		std::any oldValue;
		{
			std::lock_guard lock(mutex);
			auto it = data.find(key);
			if (it != data.end()) {
				oldValue = std::move(it->second);
				data.erase(it);
				existed = true;
			}
		}

		if (existed) {
			notifyObservers(key, oldValue);
		}
		return existed;
	}

	/// \brief Подписаться на обновление записи
	/// \param key Ключ
	/// \param observer Наблюдатель на обновление записи
	void subscribe(std::string_view key, AbstractEntryObserver *observer)
	{
		std::lock_guard lock(mutex);
		keyObservers[key].push_back(observer);
	}

	/// \brief Отписаться от обновления
	/// \param key Ключ
	/// \param observer Наблюдатель
	void unsubscribe(std::string_view key, AbstractEntryObserver *observer)
	{
		std::lock_guard lock(mutex);
		auto it = keyObservers.find(key);
		if (it != keyObservers.end()) {
			auto &observers = it->second;
			observers.erase(std::remove(observers.begin(), observers.end(), observer), observers.end());
		}
	}

	/// \brief Подписаться на обновление префикса
	/// \param prefix Префикс (например telem.)
	/// \param observer Наблюдатель
	void subscribeToPrefix(std::string_view prefix, AbstractPrefixObserver *observer)
	{
		std::lock_guard lock(mutex);
		prefixObservers[prefix].push_back(observer);
	}

	/// \brief Отписаться от обновление префикса
	/// \param prefix Префикс (например telem.)
	/// \param observer Наблюдатель
	void unsubscribeFromPrefix(std::string_view prefix, AbstractPrefixObserver *observer)
	{
		std::lock_guard lock(mutex);
		auto it = prefixObservers.find(prefix);
		if (it != prefixObservers.end()) {
			auto &observers = it->second;
			observers.erase(std::remove(observers.begin(), observers.end(), observer), observers.end());
		}
	}

	/// \brief Убрать наблюдателя на запись из всех записей
	/// \param observer Наблюдатель
	void unsubscribeAll(AbstractEntryObserver *observer)
	{
		std::lock_guard lock(mutex);
		for (auto &[key, observers] : keyObservers) {
			observers.erase(std::remove(observers.begin(), observers.end(), observer), observers.end());
		}
	}

	/// \brief Убрать наблюдателя на префикс из всех записей
	/// \param observer Наблюдатель
	void unsubscribeAll(AbstractPrefixObserver *observer)
	{
		std::lock_guard lock(mutex);
		for (auto &[prefix, observers] : prefixObservers) {
			observers.erase(std::remove(observers.begin(), observers.end(), observer), observers.end());
		}
	}

	/// \brief Получить вектор ключей по префиксу
	/// \param prefix Префикс
	/// \return вектор с ключами
	std::vector<std::string_view> getKeysByPrefix(std::string_view prefix) const
	{
		std::lock_guard lock(mutex);
		std::vector<std::string_view> result;

		for (const auto &key : data) {
			if (key.first.find(prefix) != std::string::npos) {
				result.push_back(key.first);
			}
		}

		return result;
	}

	/// \brief Получить имя типа записи
	/// \param key Ключ
	/// \return имя типа
	std::string getTypeName(std::string_view key) const
	{
		std::lock_guard lock(mutex);
		auto it = data.find(key);
		if (it != data.end()) {
			return it->second.type().name();
		}
		return "not_found";
	}

	/// \brief Распечатать все имеющиеся ключи
	void printAllKeys()
	{
		std::cout << "All BB keys:" << std::endl;
		for (const auto &pos : data) {
			std::cout << pos.first << std::endl;
		}
		std::cout << "BB keys end" << std::endl;
	}

private:

	/// \brief Оповестить все обсерверы
	/// \param key Ключ
	/// \param value any со значением
	void notifyObservers(std::string_view key, const std::any &value)
	{
		std::vector<AbstractEntryObserver *> keyObserversCopy;
		std::vector<std::pair<std::string_view, AbstractPrefixObserver *>> prefixObserversCopy;

		{
			std::lock_guard lock(mutex);

			auto keyIt = keyObservers.find(key);
			if (keyIt != keyObservers.end()) {
				keyObserversCopy = keyIt->second;
			}

			for (const auto &[prefix, observers] : prefixObservers) {
				if (key.find(prefix) != std::string::npos) {
					for (auto *observer : observers) {
						prefixObserversCopy.emplace_back(prefix, observer);
					}
				}
			}
		}

		for (auto *observer : keyObserversCopy) {
			observer->onEntryUpdated(key, value);
		}
		for (auto &[prefix, observer] : prefixObserversCopy) {
			observer->onPrefixUpdated(prefix, key, value);
		}
	}
};
