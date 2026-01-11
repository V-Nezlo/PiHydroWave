#pragma once

#include "logger/Logger.hpp"
#include <any>
#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <algorithm>
#include <vector>

struct StrHash {
  using is_transparent = void;

  size_t operator()(std::string_view s) const noexcept {
	return std::hash<std::string_view>{}(s);
  }
  size_t operator()(const std::string& s) const noexcept {
	return (*this)(std::string_view{s});
  }
  size_t operator()(const char* s) const noexcept {
	return (*this)(std::string_view{s});
  }
};

struct StrEq {
  using is_transparent = void;

  bool operator()(const std::string& a, const std::string& b) const noexcept {
	return a == b;
  }

  bool operator()(std::string_view a, std::string_view b) const noexcept {
	return a == b;
  }
  bool operator()(const std::string& a, std::string_view b) const noexcept {
	return std::string_view{a} == b;
  }
  bool operator()(std::string_view a, const std::string& b) const noexcept {
	return a == std::string_view{b};
  }
  bool operator()(const char* a, std::string_view b) const noexcept {
	return std::string_view{a} == b;
  }
  bool operator()(std::string_view a, const char* b) const noexcept {
	return a == std::string_view{b};
  }
};


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
	mutable std::recursive_mutex mutex;

	std::unordered_map<std::string_view, std::vector<AbstractEntryObserver *>> keyObservers;
	std::unordered_map<std::string_view, std::vector<AbstractPrefixObserver *>> prefixObservers;

public:
	template<typename T>
	bool set(std::string_view key, T&& value)
	{
		using V = std::decay_t<T>;

		bool changed = false;

		std::string_view key_copy;
		std::any value_copy;
		{
			std::lock_guard lock(mutex);
			auto it = data.find(key);

			// Если ключа нет - аллоцируем строку, кладем внутрь
			if (it == data.end()) {
				auto result = data.try_emplace(std::string(key), std::any(std::forward<T>(value)));
				it = result.first;
				changed = true;
			// Если ключ есть - проверим, есть ли смысл вызывать обсерверы
			} else {

				if (auto* old = std::any_cast<V>(&it->second)) {
					// Если оператор сравнения есть - сравниваем
					if constexpr (requires (const V& a, const V& b) { a == b; }) {
						if (*old != value) {
							it->second = std::any(std::forward<T>(value));
							changed = true;
						}
					// Если нет - считаем измененным
					} else {
						it->second = std::any(std::forward<T>(value));
						changed = true;
					}
				} else {
					HYDRO_LOG_ERROR("Key" + std::string(key) + "trying to change his type, daga kotowaru!");
				}
			}

			if (changed) {
				key_copy = it->first;
				value_copy = it->second;  // snapshot, чтобы после unlock не лезть в data
			}
		}

		if (changed) {
			notifyObservers(key_copy, value_copy);
		}
		return changed;
	}

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

	const std::optional<std::any> getAny(std::string_view key)
	{
		std::lock_guard lock(mutex);
		auto it = data.find(key);
		if (it != data.end()) {
			return it->second;
		}

		return std::nullopt;
	}

	template<typename T>
	T getOr(std::string_view key, T defaultValue) const
	{
		return get<T>(key).value_or(defaultValue);
	}

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

	bool has(std::string_view key) const
	{
		std::lock_guard lock(mutex);
		return data.find(key) != data.end();
	}

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

	void subscribe(std::string_view key, AbstractEntryObserver *observer)
	{
		std::lock_guard lock(mutex);
		keyObservers[key].push_back(observer);
	}

	void unsubscribe(std::string_view key, AbstractEntryObserver *observer)
	{
		std::lock_guard lock(mutex);
		auto it = keyObservers.find(key);
		if (it != keyObservers.end()) {
			auto &observers = it->second;
			observers.erase(std::remove(observers.begin(), observers.end(), observer), observers.end());
		}
	}

	void subscribeToPrefix(std::string_view prefix, AbstractPrefixObserver *observer)
	{
		std::lock_guard lock(mutex);
		prefixObservers[prefix].push_back(observer);
	}

	void unsubscribeFromPrefix(std::string_view prefix, AbstractPrefixObserver *observer)
	{
		std::lock_guard lock(mutex);
		auto it = prefixObservers.find(prefix);
		if (it != prefixObservers.end()) {
			auto &observers = it->second;
			observers.erase(std::remove(observers.begin(), observers.end(), observer), observers.end());
		}
	}

	void unsubscribeAll(AbstractEntryObserver *observer)
	{
		std::lock_guard lock(mutex);
		for (auto &[key, observers] : keyObservers) {
			observers.erase(std::remove(observers.begin(), observers.end(), observer), observers.end());
		}
	}

	void unsubscribeAll(AbstractPrefixObserver *observer)
	{
		std::lock_guard lock(mutex);
		for (auto &[prefix, observers] : prefixObservers) {
			observers.erase(std::remove(observers.begin(), observers.end(), observer), observers.end());
		}
	}

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

	std::string getTypeName(std::string_view key) const
	{
		std::lock_guard lock(mutex);
		auto it = data.find(key);
		if (it != data.end()) {
			return it->second.type().name();
		}
		return "not_found";
	}

	void printAllKeys()
	{
		std::cout << "All BB keys:" << std::endl;
		for (const auto &pos : data) {
			std::cout << pos.first << std::endl;
		}
		std::cout << "BB keys end" << std::endl;
	}

private:
	void notifyObservers(std::string_view key, const std::any &value)
	{
		std::vector<AbstractEntryObserver *> keyObserversCopy;
		std::vector<std::pair<std::string_view, AbstractPrefixObserver *>> prefixObserversCopy;
		{
			std::lock_guard lock(mutex);

			// Копируем наблюдатели для конкретного ключа
			auto keyIt = keyObservers.find(key);
			if (keyIt != keyObservers.end()) {
				keyObserversCopy = keyIt->second;
			}

			// Копируем наблюдатели для префиксов
			for (const auto &[prefix, observers] : prefixObservers) {
				if (key.find(prefix) != std::string::npos) {
					for (auto *observer : observers) {
						prefixObserversCopy.emplace_back(prefix, observer);
					}
				}
			}
		}

		// Уведомляем наблюдатели вне мьютекса
		for (auto *observer : keyObserversCopy) {
			observer->onEntryUpdated(key, value);
		}
		for (auto &[prefix, observer] : prefixObserversCopy) {
			observer->onPrefixUpdated(prefix, key, value);
		}
	}
};
