#pragma once

#include <any>
#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

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
	std::unordered_map<std::string, std::any> data;
	mutable std::recursive_mutex mutex;

	std::unordered_map<std::string, std::vector<AbstractEntryObserver *>> keyObservers;
	std::unordered_map<std::string, std::vector<AbstractPrefixObserver *>> prefixObservers;

public:
	template<typename T>
	bool set(std::string_view key, T &&value)
	{
		using ValueType = std::decay_t<T>;
		bool changed = false;
		{
			std::lock_guard lock(mutex);
			std::string keyStr(key);

			auto it = data.find(keyStr);
			if (it != data.end()) {
				// Сравниваем только тип, не значение (убираем проблему с оператором ==)
				if (it->second.type() != typeid(ValueType)) {
					// Тип изменился - считаем что значение изменилось
					it->second = std::forward<T>(value);
					changed = true;
				} else {
					// Тип тот же - сохраняем старое поведение с == если возможно
					if constexpr (requires { std::declval<ValueType>() == std::declval<ValueType>(); }) {
						if (auto *oldVal = std::any_cast<ValueType>(&it->second)) {
							if (*oldVal != value) {
								it->second = std::forward<T>(value);
								changed = true;
							}
						}
					} else {
						// Для типов без оператора == всегда считаем измененным
						it->second = std::forward<T>(value);
						changed = true;
					}
				}
			} else {
				data[std::move(keyStr)] = std::forward<T>(value);
				changed = true;
			}
		}

		if (changed) {
			notifyObservers(key, data[std::string(key)]);
		}
		return changed;
	}

	// Получение с проверкой типа
	template<typename T>
	std::optional<T> get(std::string_view key) const
	{
		std::lock_guard lock(mutex);
		std::string keyStr(key);
		auto it = data.find(keyStr);
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

	template<typename T>
	T getOr(std::string_view key, T defaultValue) const
	{
		return get<T>(key).value_or(defaultValue);
	}

	template<typename T>
	bool isType(std::string_view key) const
	{
		std::lock_guard lock(mutex);
		std::string keyStr(key);
		auto it = data.find(keyStr);
		if (it != data.end()) {
			return it->second.type() == typeid(T);
		}
		return false;
	}

	bool has(std::string_view key) const
	{
		std::lock_guard lock(mutex);
		std::string keyStr(key);
		return data.find(keyStr) != data.end();
	}

	bool remove(std::string_view key)
	{
		bool existed = false;
		std::any oldValue;
		{
			std::lock_guard lock(mutex);
			std::string keyStr(key);
			auto it = data.find(keyStr);
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

	// Подписка на конкретный ключ
	void subscribe(std::string_view key, AbstractEntryObserver *observer)
	{
		std::lock_guard lock(mutex);
		keyObservers[std::string(key)].push_back(observer);
	}

	// Отписка от конкретного ключа
	void unsubscribe(std::string_view key, AbstractEntryObserver *observer)
	{
		std::lock_guard lock(mutex);
		auto it = keyObservers.find(std::string(key));
		if (it != keyObservers.end()) {
			auto &observers = it->second;
			observers.erase(std::remove(observers.begin(), observers.end(), observer), observers.end());
		}
	}

	// Подписка на префикс
	void subscribeToPrefix(std::string_view prefix, AbstractPrefixObserver *observer)
	{
		std::lock_guard lock(mutex);
		prefixObservers[std::string(prefix)].push_back(observer);
	}

	// Отписка от префикса
	void unsubscribeFromPrefix(std::string_view prefix, AbstractPrefixObserver *observer)
	{
		std::lock_guard lock(mutex);
		auto it = prefixObservers.find(std::string(prefix));
		if (it != prefixObservers.end()) {
			auto &observers = it->second;
			observers.erase(std::remove(observers.begin(), observers.end(), observer), observers.end());
		}
	}

	// Отписка наблюдателя от всех ключей и префиксов
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

	std::vector<std::string> getKeysByPrefix(std::string_view prefix) const
	{
		std::lock_guard lock(mutex);
		std::vector<std::string> result;
		std::string prefixStr(prefix);

		for (const auto &[key, _] : data) {
			if (key.find(prefixStr) == 0) {
				result.push_back(key);
			}
		}
		return result;
	}

	std::string getTypeName(std::string_view key) const
	{
		std::lock_guard lock(mutex);
		std::string keyStr(key);
		auto it = data.find(keyStr);
		if (it != data.end()) {
			return it->second.type().name();
		}
		return "not_found";
	}

private:
	void notifyObservers(std::string_view key, const std::any &value)
	{
		std::vector<AbstractEntryObserver *> keyObserversCopy;
		std::vector<std::pair<std::string, AbstractPrefixObserver *>> prefixObserversCopy;

		{
			std::lock_guard lock(mutex);
			std::string keyStr(key);

			// Копируем наблюдатели для конкретного ключа
			auto keyIt = keyObservers.find(keyStr);
			if (keyIt != keyObservers.end()) {
				keyObserversCopy = keyIt->second;
			}

			// Копируем наблюдатели для префиксов
			for (const auto &[prefix, observers] : prefixObservers) {
				if (keyStr.find(prefix) == 0) {
					for (auto *observer : observers) { prefixObserversCopy.emplace_back(prefix, observer); }
				}
			}
		}

		// Уведомляем наблюдатели вне мьютекса
		for (auto *observer : keyObserversCopy) { observer->onEntryUpdated(key, value); }
		for (auto &[prefix, observer] : prefixObserversCopy) { observer->onPrefixUpdated(prefix, key, value); }
	}
};
