#pragma once

#include <iostream>
#include <optional>
#include <string>
#include <cstdlib>

struct Args {
	std::string interfacePath;             // -i
	std::string configPath;                // -c
	std::optional<std::string> dbPath;     // -db
	unsigned logLevel = 0;                 // -D
};

Args parseArgs(int argc, char* argv[]) {
	Args result;

	bool hasInterface = false;
	bool hasConfig = false;

	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];

		if (arg == "-i") {
			if (i + 1 >= argc) {
				std::cerr << "Ошибка: флаг -i требует путь до интерфейса\n";
				std::exit(1);
			}
			result.interfacePath = argv[++i];
			hasInterface = true;
		}

		else if (arg == "-c") {
			if (i + 1 >= argc) {
				std::cerr << "Ошибка: флаг -c требует путь до config-файла\n";
				std::exit(1);
			}
			result.configPath = argv[++i];
			hasConfig = true;
		}

		else if (arg == "-db") {
			if (i + 1 >= argc) {
				std::cerr << "Ошибка: флаг -db требует путь до базы\n";
				std::exit(1);
			}
			result.dbPath = std::string(argv[++i]);
		}

		else if (arg == "-D") {
			if (i + 1 >= argc) {
				std::cerr << "Ошибка: флаг -D требует числовой аргумент\n";
				std::exit(1);
			}
			try {
				result.logLevel = static_cast<unsigned>(std::stoul(argv[++i]));
			} catch (...) {
				std::cerr << "Ошибка: значение для -D должно быть unsigned числом\n";
				std::exit(1);
			}
		}

		else {
			std::cerr << "Неизвестный аргумент: " << arg << "\n";
			std::exit(1);
		}
	}

	// Проверка обязательных аргументов
	if (!hasInterface) {
		std::cerr << "Ошибка: необходимо задать интерфейс через -i\n";
		std::exit(1);
	}

	if (!hasConfig) {
		std::cerr << "Ошибка: необходимо задать конфиг через -c\n";
		std::exit(1);
	}

	// Предупреждение при отсутствии базы
	if (!result.dbPath.has_value()) {
		std::cerr << "Предупреждение: путь к базе данных (-db) не указан, работа продолжается\n";
	}

	return result;
}
