#ifndef INTERFACELIST_HPP
#define INTERFACELIST_HPP

#include <cstddef>
#include <cstdint>

class AbstractSerial {
public:
	virtual size_t write(const uint8_t *aData, size_t aLength) = 0;
	virtual size_t read(void *aData, size_t aLen) = 0;
};

#endif // INTERFACELIST_HPP
