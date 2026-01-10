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

// 1) Регистрация настройки — только схема + дефолты
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

// 2) Загрузка конфигурации
bool SettingsManager::load()
{
	// 0) Собираем базу: дефолты
	std::unordered_map<std::string, SettingValue> effective = defaults;

	// 1) Если файла нет — просто приводим типы и создаём
	if (!std::filesystem::exists(filename)) {
		std::cout << "Config not found. Creating default config.\n";

		adjustTypes(effective);
		writeToBlackboard(effective);
		save();

		initialized = true;
		return true;
	}

	// 2) Если файл есть — читаем и подмешиваем значения
	try {
		std::ifstream file(filename);
		json config = json::parse(file);

		// 2.1) Проверяем схему
		if (!validateSchema(config)) {

			std::cout << "Schema mismatch. Recreating config with defaults.\n";

			adjustTypes(effective);
			writeToBlackboard(effective);
			save();

			initialized = true;
			return true;
		}

		// 2.2) Подмешиваем сохранённые значения (пока как "сырьё")
		if (config.contains("values") && config["values"].is_object()) {
			for (auto &[key, jval] : config["values"].items()) {
				if (!schema.contains(key))
					continue;

				const auto &def = schema.at(key);

				try {
					switch (def.type) {
						case SettingType::BOOL:
							effective[key] = jval.get<bool>();
							break;

						case SettingType::INT:
							effective[key] = jval.get<int>();
							break;

						case SettingType::FLOAT:
							effective[key] = jval.get<float>();
							break;

						case SettingType::STRING:
							effective[key] = jval.get<std::string>();
							break;

						case SettingType::SECONDS:
						case SettingType::MSECONDS:
							// Будет переопределено в adjust
							effective[key] = jval.get<int>();
							break;
					}
				} catch (...) {
					// fallback на дефолт
					effective[key] = def.defaultValue;
				}
			}
		}

		// 2.3) Приводим всё к типам схемы (тут int -> seconds/ms)
		adjustTypes(effective);

		// 2.4) Пишем merged-значения в blackboard
		writeToBlackboard(effective);

		initialized = true;
		std::cout << "Config loaded successfully.\n";
		return true;

	} catch (const std::exception &e) {
		std::cerr << "Error loading config: " << e.what() << "\n";

		// 3) Если файл битый/не парсится — всё равно поднимаемся на дефолтах
		adjustTypes(effective);
		writeToBlackboard(effective);
		save();

		initialized = true;
		return false;
	}
}


// 3) Автосохранение, если BB изменился
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

// Проверка схемы: ключи + типы
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
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same_v<T, std::chrono::seconds>) {
				d["default"] = arg.count();
				d["unit"] = "s";
			} else if constexpr (std::is_same_v<T, std::chrono::milliseconds>) {
				d["default"] = arg.count();
				d["unit"] = "ms";
			} else {
				d["default"] = json(arg);
			}
		}, def.defaultValue);

		out[key] = d;
	}

	return out;
}

json SettingsManager::getConfigForSaving() const
{
	json cfg;
	cfg["schema"] = generateSchema();

	json vals;

	for (const auto &[key, def] : schema) {
		switch (def.type) {
			case SettingType::BOOL:
				if (auto v = blackboard->get<bool>(key)) vals[key] = *v;
				break;

			case SettingType::INT:
				if (auto v = blackboard->get<int>(key)) vals[key] = *v;
				break;

			case SettingType::FLOAT:
				if (auto v = blackboard->get<float>(key)) vals[key] = *v;
				break;

			case SettingType::STRING:
				if (auto v = blackboard->get<std::string>(key)) vals[key] = *v;
				break;

			case SettingType::SECONDS:
				if (auto v = blackboard->get<std::chrono::seconds>(key)) vals[key] = v->count();
				break;

			case SettingType::MSECONDS:
				if (auto v = blackboard->get<std::chrono::milliseconds>(key)) vals[key] = v->count();
				break;
		}
	}

	cfg["values"] = vals;
	return cfg;
}


// Конвертация типа в строку
std::string SettingsManager::toString(SettingType type)
{
	switch (type) {
		case SettingType::BOOL:     return "bool";
		case SettingType::INT:      return "int";
		case SettingType::FLOAT:    return "float";
		case SettingType::STRING:   return "string";
		case SettingType::SECONDS:  return "seconds";
		case SettingType::MSECONDS: return "milliseconds";
	}
	return "unknown";
}

void SettingsManager::adjustTypes(std::unordered_map<std::string, SettingValue> &effective)
{
	for (const auto &[key, def] : schema) {
		// если ключа нет — подставляем дефолт (на всякий)
		auto it = effective.find(key);
		if (it == effective.end()) {
			effective[key] = def.defaultValue;
			continue;
		}

		auto &v = it->second;

		auto asInt = [&]() -> std::optional<int> {
			if (auto p = std::get_if<int>(&v)) return *p;
			if (auto p = std::get_if<float>(&v)) return static_cast<int>(*p);
			if (auto p = std::get_if<bool>(&v)) return *p ? 1 : 0;
			return std::nullopt;
		};

		switch (def.type) {
			case SettingType::SECONDS: {
				if (std::holds_alternative<std::chrono::seconds>(v)) break;
				auto n = asInt().value_or(0);
				if (n < 0) n = 0;
				v = std::chrono::seconds{n};
				break;
			}

			case SettingType::MSECONDS: {
				if (std::holds_alternative<std::chrono::milliseconds>(v)) break;
				auto n = asInt().value_or(0);
				if (n < 0) n = 0;
				v = std::chrono::milliseconds{n};
				break;
			}

			default:
				break;
		}
	}
}
