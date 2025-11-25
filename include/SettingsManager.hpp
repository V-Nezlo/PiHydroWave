#pragma once

#include "core/Blackboard.hpp"

#include <array>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

using json = nlohmann::json;

enum class SettingType { BOOL, INT, FLOAT, U64, UNSIGNED };
using SettingValue = std::variant<bool, int, double, uint64_t>;

struct SettingDefinition {
	std::string key;
	SettingType type;
	SettingValue defaultValue;
	std::string description;
};

class SettingsManager : public AbstractEntryObserver {
public:
	SettingsManager(std::shared_ptr<Blackboard> blackboard,
					const std::string &filename = "config.json",
					bool autoSave = true);
	SettingsManager(const SettingsManager &) = delete;
	SettingsManager &operator=(const SettingsManager &) = delete;
	~SettingsManager() override;

	// Регистрация настроек
	void registerSetting(const std::string &key, SettingType type,
						 SettingValue defaultValue, const std::string &desc = "");

	// Работа с конфигом
	bool load();
	bool save();

	// Коллбек из Blackboard
	void onEntryUpdated(std::string_view entry, const std::any &value) override;

private:
	// Внутренние инструменты
	bool validateSchema(const json &existingConfig) const;
	void writeToBlackboard(const std::unordered_map<std::string, SettingValue> &vals);

	json generateSchema() const;
	json getConfigForSaving() const;

	static std::string toString(SettingType type);

private:
	std::unordered_map<std::string, SettingDefinition> schema;
	std::unordered_map<std::string, SettingValue> defaults;

	std::string filename;
	std::shared_ptr<Blackboard> blackboard;

	bool autoSave;
	bool loadingProcess;
	bool initialized{false};
};
