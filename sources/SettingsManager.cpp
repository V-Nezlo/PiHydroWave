#include "SettingsManager.hpp"
#include <filesystem>
#include <iostream>
#include <string>

SettingsManager::SettingsManager(std::shared_ptr<Blackboard> aBlackboard, const std::string &aFilename, bool aAutoSave) :
	filename(aFilename), blackboard(aBlackboard), autoSave(aAutoSave), loadingProcess(false)
{ }

SettingsManager::~SettingsManager()
{
	for (const auto &[key, _] : schema) { blackboard->unsubscribe(key, this); }
}

// Базовый метод для простых типов
void SettingsManager::registerSetting(
	const std::string &key, SettingType type, SettingValue defaultValue, const std::string &desc)
{
	SettingDefinition def{key, type, std::move(defaultValue), desc};
	schema[key] = def;
	blackboard->subscribe(key, this);

	std::visit([this, &key](auto &&value) { blackboard->set(key, value); }, def.defaultValue);
}

bool SettingsManager::load()
{
	if (!std::filesystem::exists(filename)) {
		std::cout << "Config file not found, creating default" << std::endl;
		pushAllToBlackboard();
		return save();
	}

	try {
		std::ifstream file(filename);
		json config = json::parse(file);

		if (!validateSchema(config)) {
			std::cout << "Schema validation failed, recreating config" << std::endl;
			pushAllToBlackboard();
			return save();
		}

		loadingProcess = true;

		if (config.contains("values")) {
			// Загружаем обычные настройки
			for (auto &[key, value] : config["values"].items()) {
				if (schema.count(key)) {
					applySettingToBlackboard(key, value);
				}
			}
		}

		loadingProcess = false;
		std::cout << "Config loaded successfully" << std::endl;
		return true;

	} catch (const std::exception &e) {
		std::cerr << "Error loading config: " << e.what() << std::endl;
		loadingProcess = false;
		pushAllToBlackboard();
		return save();
	}
}

bool SettingsManager::save()
{
	try {
		json config = getConfigForSaving();
		std::ofstream file(filename);
		file << config.dump(4);
		std::cout << "Config saved successfully" << std::endl;
		return true;
	} catch (const std::exception &e) {
		std::cerr << "Error saving config: " << e.what() << std::endl;
		return false;
	}
}

void SettingsManager::onEntryUpdated(std::string_view entry, const std::any & /*value*/)
{
	if (loadingProcess)
		return;

	std::string key(entry);

	// Проверяем как обычную настройку
	if (schema.count(key) != 0) {
		std::cout << "Blackboard update for setting: " << key << std::endl;
		if (autoSave)
			save();
	}
}

void SettingsManager::pushAllToBlackboard()
{
	// Устанавливаем значения по умолчанию для обычных настроек
	for (const auto &[key, def] : schema) {
		std::visit([this, &key](auto &&arg) { blackboard->set(key, arg); }, def.defaultValue);
	}
}

void SettingsManager::applySettingToBlackboard(const std::string &key, const json &value)
{
	const auto &def = schema.at(key);

	std::visit(
		[this, &key, &value](auto &&defaultVal) {
			using ValueType = std::decay_t<decltype(defaultVal)>;

			try {
				ValueType parsedValue = value.get<ValueType>();
				blackboard->set(key, parsedValue);
			} catch (const std::exception &e) {
				std::cerr << "Error parsing setting '" << key << "': " << e.what() << ", using default value"
						  << std::endl;
				blackboard->set(key, defaultVal);
			}
		},
		def.defaultValue);
}

bool SettingsManager::validateSchema(const json &existingConfig) const
{
	if (!existingConfig.contains("schema") || !existingConfig["schema"].is_object()) {
		return false;
	}

	const json &existingSchema = existingConfig["schema"];
	for (const auto &[key, def] : schema) {
		if (!existingSchema.contains(key))
			return false;
		const json &existingDef = existingSchema[key];
		if (!existingDef.contains("type") || existingDef["type"] != toString(def.type)) {
			return false;
		}
	}
	return true;
}

json SettingsManager::generateSchema() const
{
	json newSchema;
	for (const auto &[key, def] : schema) {
		json defJson;
		defJson["type"] = toString(def.type);
		defJson["description"] = def.description;

		// Сериализуем значение по умолчанию
		std::visit([&defJson](auto &&arg) { defJson["default"] = json(arg); }, def.defaultValue);
		newSchema[key] = defJson;
	}
	return newSchema;
}

json SettingsManager::getConfigForSaving() const
{
	json config;
	config["schema"] = generateSchema();

	// Сохраняем обычные настройки
	json values;
	for (const auto &[key, def] : schema) {
		std::visit(
			[&values, &key, this](auto &&arg) {
				using ValueType = std::decay_t<decltype(arg)>;
				if (auto value = blackboard->get<ValueType>(key)) {
					values[key] = *value;
				}
			},
			def.defaultValue);
	}
	config["values"] = values;
	return config;
}

std::string SettingsManager::toString(SettingType type)
{
	switch (type) {
		case SettingType::BOOL:
			return "bool";
		case SettingType::INT:
			return "int";
		case SettingType::FLOAT:
			return "float";
		case SettingType::U64:
			return "uint64";
		default:
			return "unknown";
	}
}
