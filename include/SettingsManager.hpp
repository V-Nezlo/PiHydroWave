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

enum class SettingType { BOOL, INT, FLOAT, U64, UNSIGNED};
using SettingValue = std::variant<bool, int, double, uint64_t>;

struct SettingDefinition {
	std::string key;
	SettingType type;
	SettingValue defaultValue;
	std::string description;
};

class SettingsManager : public AbstractEntryObserver {
private:
	std::unordered_map<std::string, SettingDefinition> schema;
	std::string filename;
	std::shared_ptr<Blackboard> blackboard;
	bool autoSave;
	bool loadingProcess;

public:
	SettingsManager(std::shared_ptr<Blackboard> blackboard, const std::string &filename = "config.json", bool autoSave = true);
	SettingsManager(const SettingsManager &) = delete;
	SettingsManager &operator=(const SettingsManager &) = delete;
	~SettingsManager() override;

	// Базовые методы для простых типов
	void registerSetting(
		const std::string &key, SettingType type, SettingValue defaultValue, const std::string &desc = "");

	// Работа с конфигом
	bool load();
	bool save();
	bool validateSchema(const json &existingConfig) const;
	void onEntryUpdated(std::string_view entry, const std::any &value) override;
	void pushAllToBlackboard();

private:
	json generateSchema() const;
	json getConfigForSaving() const;
	void applySettingToBlackboard(const std::string &key, const json &value);

	// Вспомогательные
	static std::string toString(SettingType type);
};
