#pragma once

#include "core/InterfaceList.hpp"
#include <any>
#include <string>

class MacFieldValidator : public AbstractValidator {
public:
	// AbstractValidator interface
	bool isDataCorrect(const std::any &aValue) const override
	{
		try {
			const std::string value = std::any_cast<std::string>(aValue);

			if (value.size() != 17) {
				return false;
			}

			size_t colonCount = 0;

			for (size_t i = 0; i < value.size(); ++i) {
				if (value[i] == ':') {
					++colonCount;

					if ((i % 3) != 2) {
						return false;
					}
				}
				else {
					if (!std::isxdigit(static_cast<unsigned char>(value[i]))) {
						return false;
					}
				}
			}

			return colonCount == 5;
		}
		catch (...) {
			return false;
		}
	}
};
