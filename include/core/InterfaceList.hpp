#pragma once

#include <any>
#include <cstddef>
#include <cstdint>

class AbstractSerial {
public:
	virtual ~AbstractSerial() = default;
	virtual bool open() = 0;
	virtual void close() = 0;
	virtual bool opened() const = 0 ;
	virtual bool ping() = 0;
	virtual size_t write(const uint8_t *aData, size_t aLength) = 0;
	virtual size_t read(void *aData, size_t aLen) = 0;
};

class AbstractValidator {
public:
	virtual bool isDataCorrect(const std::any &aValue) const = 0;
};
