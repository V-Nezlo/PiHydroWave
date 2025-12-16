#include "SettingsManager.hpp"
#include <filesystem>
#include <iostream>

SettingsManager::SettingsManager(std::shared_ptr<Blackboard> aBlackboard,
								 const std::string &aFilename,
								 bool aAutoSave)
	: filename(aFilename),
	blackboard(aBlackboard),
	autoSave(aAutoSave),
	loadingProcess(false),
	initialized(false)
{
}

SettingsManager::~SettingsManager()
{
	for (const auto &[key, _] : schema) {
		blackboard->unsubscribe(key, this);
	}
}

//
// 1) Регистрация настройки — только схема + дефолты
//
void SettingsManager::registerSetting(
	const std::string &key, SettingType type,
	SettingValue defaultValue, const std::string &desc)
{
	SettingDefinition def{key, type, defaultValue, desc};
	schema[key] = def;
	defaults[key] = defaultValue;

	// Подписываемся на обновления, но BB пока не трогаем
	blackboard->subscribe(key, this);
}

//
// 2) Загрузка конфигурации
//
bool SettingsManager::load()
{
	// Собираем рабочий набор значений: дефолты
	std::unordered_map<std::string, SettingValue> effective = defaults;

	if (!std::filesystem::exists(filename)) {
		std::cout << "Config not found. Creating default config.\n";
		writeToBlackboard(effective);
		save();
		initialized = true;
		return true;
	}

	try {
		std::ifstream file(filename);
		json config = json::parse(file);

		// Проверяем схему
		if (!validateSchema(config)) {
			std::cout << "Schema mismatch. Recreating config with defaults.\n";
			writeToBlackboard(effective);
			save();
			initialized = true;
			return true;
		}

		// Подмешиваем сохранённые значения
		if (config.contains("values")) {
			for (auto &[key, val] : config["values"].items()) {
				if (!schema.contains(key))
					continue;

				const auto &def = schema[key];

				std::visit(
					[&](auto &&dval) {
						using T = std::decay_t<decltype(dval)>;
						try {
							T parsed = val.get<T>();
							effective[key] = parsed;
						} catch (...) {
							effective[key] = dval;
						}
					},
					def.defaultValue);
			}
		}

		// Пишем merged-значения в blackboard
		writeToBlackboard(effective);
		initialized = true;

		std::cout << "Config loaded successfully.\n";
		return true;

	} catch (const std::exception &e) {
		std::cerr << "Error loading config: " << e.what() << "\n";

		writeToBlackboard(effective);
		save();
		initialized = true;

		return false;
	}
}

//
// 3) Автосохранение, если BB изменился
//
void SettingsManager::onEntryUpdated(std::string_view entry, const std::any & /*value*/)
{
	if (loadingProcess || !initialized)
		return;

	const std::string key(entry);

	if (schema.count(key) != 0 && autoSave) {
		save();
	}
}

//
// 4) Сохранение текущего состояния BB
//
bool SettingsManager::save()
{
	try {
		json config = getConfigForSaving();
		std::ofstream file(filename);
		file << config.dump(4);
		return true;
	} catch (const std::exception &e) {
		std::cerr << "Error saving config: " << e.what() << "\n";
		return false;
	}
}

//
// Вспомогательная запись в BB
//
void SettingsManager::writeToBlackboard(
	const std::unordered_map<std::string, SettingValue> &vals)
{
	loadingProcess = true;

	for (const auto &[key, val] : vals) {
		std::visit(
			[&](auto &&arg) {
				blackboard->set(key, arg);
			},
			val);
	}

	loadingProcess = false;
}

//
// Проверка схемы: ключи + типы
//
bool SettingsManager::validateSchema(const json &existingConfig) const
{
	if (!existingConfig.contains("schema") || !existingConfig["schema"].is_object())
		return false;

	const json &existingSchema = existingConfig["schema"];

	for (const auto &[key, def] : schema) {
		if (!existingSchema.contains(key))
			return false;

		const json &edef = existingSchema[key];
		if (!edef.contains("type"))
			return false;

		if (edef["type"] != toString(def.type))
			return false;
	}

	return true;
}

//
// Сбор схемы
//
json SettingsManager::generateSchema() const
{
	json out;
	for (const auto &[key, def] : schema) {
		json d;
		d["type"] = toString(def.type);
		d["description"] = def.description;

		std::visit([&](auto &&arg) {
			d["default"] = json(arg);
		}, def.defaultValue);

		out[key] = d;
	}
	return out;
}

//
// Сбор JSON для сохранения
//
json SettingsManager::getConfigForSaving() const
{
	json cfg;
	cfg["schema"] = generateSchema();

	json vals;

	for (const auto &[key, def] : schema) {
		std::visit(
			[&](auto &&arg) {
				using T = std::decay_t<decltype(arg)>;
				if (auto v = blackboard->get<T>(key)) {
					vals[key] = *v;
				}
			},
			def.defaultValue);
	}

	cfg["values"] = vals;
	return cfg;
}

//
// Конвертация типа в строку
//
std::string SettingsManager::toString(SettingType type)
{
	switch (type) {
		case SettingType::BOOL:     return "bool";
		case SettingType::INT:      return "int";
		case SettingType::FLOAT:    return "float";
		case SettingType::U64:      return "uint64";
		case SettingType::UNSIGNED: return "unsigned";
		case SettingType::STRING:   return "string";
	}
	return "unknown";
}
