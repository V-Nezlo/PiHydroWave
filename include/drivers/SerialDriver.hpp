#pragma once

#include "core/InterfaceList.hpp"

#include <asm-generic/errno.h>
#include <cerrno>
#include <fcntl.h>
#include <optional>
#include <sys/poll.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <cstring>
#include <stdexcept>
#include <string>
#include <array>
#include <cstdint>

class SerialDriver : public AbstractSerial {
public:
	SerialDriver(std::string &aDevice, speed_t aBaud = B115200):
		fd{-1}
	{
		fd = open(aDevice.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);

		if (fd < 0) {
			throw std::runtime_error("Failed to open serial port" + aDevice + ": " + strerror(errno));
		}

		struct termios tty{};
		if (tcgetattr(fd, &tty) != 0) {
			throw std::runtime_error("tcgetattr() failed");
		}

		cfmakeraw(&tty);
		cfsetispeed(&tty, aBaud);
		cfsetospeed(&tty, aBaud);

		tty.c_cflag |= (CLOCAL | CREAD); // enable receiver, local mode
		tty.c_cflag &= ~CRTSCTS;         // no hardware flow control
		tty.c_cflag &= ~PARENB;          // no parity
		tty.c_cflag &= ~CSTOPB;          // one stop bit
		tty.c_cflag &= ~CSIZE;
		tty.c_cflag |= CS8;              // 8 data bits

		tty.c_cc[VMIN]  = 0;
		tty.c_cc[VTIME] = 0;

		if (tcsetattr(fd, TCSANOW, &tty) != 0) {
			close(fd);
			throw std::runtime_error("tcsetattr() failed");
		}
	}

	~SerialDriver()
	{
		if (fd >= 0) {
			close(fd);
		}
	}

	size_t bytesAvaillable()
	{
		int count = 0;
		if (ioctl(fd, FIONREAD, &count) < 0) {
			return 0;
		}

		return static_cast<size_t>(count);
	}

	size_t read(void *aData, size_t aLen) override
	{
		ssize_t result = ::read(fd, aData, aLen);
		if (result < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				return 0;
			}

			throw std::runtime_error("Serial read failed: " + std::string(strerror(errno)));
		}

		return static_cast<size_t>(result);
	}

	/// \brief Враппер должен уметь в микропротокол EspNowUSB
	/// \param aData
	/// \param aLength
	/// \return
	size_t write(const uint8_t *aData, size_t aLength) override
	{
		if (fd < 0) {
			throw std::runtime_error("Serial not open");
		}

		ssize_t ret = ::write(fd, aData, aLength);

		if (ret < 0) {
			throw std::runtime_error("Serial write() failed: " + std::string(strerror(errno)));
		}

		return static_cast<size_t>(ret);
	}

	bool poll(int aTimeout = -1)
	{
		struct pollfd pfd{};
		pfd.fd = fd;
		pfd.events = POLLIN;

		int ret = ::poll(&pfd, 1, aTimeout);
		if (ret < 0) {
			throw std::runtime_error("poll() failed: " + std::string(strerror(errno)));
		}

		return ret > 0 && (pfd.revents & POLLIN);
	}

private:
	int fd;
};

