#pragma once

#include "core/InterfaceList.hpp"

#include <string>
#include <cstring>
#include <cerrno>
#include <stdexcept>

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

class SerialDriver : public AbstractSerial {
public:
	explicit SerialDriver(const std::string& aDevice, speed_t aBaud = B115200)
		: m_device(aDevice), m_baud(aBaud), fd(-1)
	{
		open();
	}

	~SerialDriver() override
	{
		close();
	}

	// Можно логировать снаружи, если open/ping вернули false.
	const std::string& lastError() const { return m_lastError; }

	bool open() override
	{
		if (fd >= 0) {
			return true;
		}

		m_lastError.clear();

		// Быстрая проверка наличия ноды (не обязательна, но помогает не спамить errno).
		struct stat st {};
		if (::stat(m_device.c_str(), &st) != 0) {
			m_lastError = "Device node not found: " + m_device + " (" + std::string(::strerror(errno)) + ")";
			return false;
		}

		int newFd = ::open(m_device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
		if (newFd < 0) {
			m_lastError = "Failed to open serial port " + m_device + ": " + std::string(::strerror(errno));
			return false;
		}

		if (!configureTermios(newFd)) {
			::close(newFd);
			return false;
		}

		fd = newFd;
		return true;
	}

	void close() override
	{
		if (fd >= 0) {
			::close(fd);
			fd = -1;
		}
	}

	bool opened() const override
	{
		return fd >= 0;
	}

	bool ping() override
	{
		if (fd >= 0) {
			// Быстрая проверка: не пришёл ли HUP/ERR/NVAL
			struct pollfd pfd {};
			pfd.fd = fd;
			pfd.events = POLLIN;

			int pr = ::poll(&pfd, 1, 0);
			if (pr < 0) {
				m_lastError = "poll() failed: " + std::string(::strerror(errno));
				close();
				return false;
			}

			if (pr > 0 && (pfd.revents & (POLLHUP | POLLERR | POLLNVAL))) {
				m_lastError = "Serial disconnected (poll revents=" + std::to_string(pfd.revents) + ")";
				close();
				return false;
			}

			struct termios tmp {};
			if (::tcgetattr(fd, &tmp) != 0) {
				if (isDisconnectErrno(errno)) {
					m_lastError = "Serial disconnected (tcgetattr): " + std::string(::strerror(errno));
					close();
					return false;
				}
				m_lastError = "tcgetattr() failed: " + std::string(::strerror(errno));
			}

			return true;
		}

		return open();
	}

	size_t bytesAvaillable()
	{
		if (fd < 0) {
			return 0;
		}

		int count = 0;
		if (::ioctl(fd, FIONREAD, &count) < 0) {
			if (isDisconnectErrno(errno)) {
				m_lastError = "FIONREAD failed (disconnect): " + std::string(::strerror(errno));
				close();
			}
			return 0;
		}

		return (count > 0) ? static_cast<size_t>(count) : 0u;
	}

	size_t read(void* aData, size_t aLen) override
	{
		if (fd < 0 || aLen == 0) {
			return 0;
		}

		ssize_t result = ::read(fd, aData, aLen);
		if (result > 0) {
			return static_cast<size_t>(result);
		}

		if (result == 0) {
			return 0;
		}

		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return 0;
		}

		if (isDisconnectErrno(errno)) {
			m_lastError = "Serial read disconnect: " + std::string(::strerror(errno));
			close();
			return 0;
		}

		close();
		return 0;
	}

	size_t write(const uint8_t* aData, size_t aLength) override
	{
		if (fd < 0 || aLength == 0) {
			return 0;
		}

		ssize_t ret = ::write(fd, aData, aLength);
		if (ret >= 0) {
			return static_cast<size_t>(ret);
		}

		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return 0;
		}

		if (isDisconnectErrno(errno)) {
			m_lastError = "Serial write disconnect: " + std::string(::strerror(errno));
			close();
			return 0;
		}

		m_lastError = "Serial write failed: " + std::string(::strerror(errno));
		return 0;
	}

	bool poll(int aTimeout = -1)
	{
		if (fd < 0) {
			return false;
		}

		struct pollfd pfd {};
		pfd.fd = fd;
		pfd.events = POLLIN;

		int ret = ::poll(&pfd, 1, aTimeout);
		if (ret < 0) {
			if (isDisconnectErrno(errno)) {
				m_lastError = "poll() disconnect: " + std::string(::strerror(errno));
				close();
				return false;
			}
			throw std::runtime_error("poll() failed: " + std::string(::strerror(errno)));
		}

		if (ret == 0) {
			return false; // timeout
		}

		if (pfd.revents & (POLLHUP | POLLERR | POLLNVAL)) {
			m_lastError = "Serial disconnected (poll revents=" + std::to_string(pfd.revents) + ")";
			close();
			return false;
		}

		return (pfd.revents & POLLIN) != 0;
	}

private:
	bool configureTermios(int aFd)
	{
		struct termios tty {};
		if (::tcgetattr(aFd, &tty) != 0) {
			m_lastError = "tcgetattr() failed: " + std::string(::strerror(errno));
			return false;
		}

		::cfmakeraw(&tty);
		::cfsetispeed(&tty, m_baud);
		::cfsetospeed(&tty, m_baud);

		tty.c_cflag |= (CLOCAL | CREAD);
		tty.c_cflag &= ~CRTSCTS;
		tty.c_cflag &= ~PARENB;
		tty.c_cflag &= ~CSTOPB;
		tty.c_cflag &= ~CSIZE;
		tty.c_cflag |= CS8;

		tty.c_cc[VMIN]  = 0;
		tty.c_cc[VTIME] = 0;

		if (::tcsetattr(aFd, TCSANOW, &tty) != 0) {
			m_lastError = "tcsetattr() failed: " + std::string(::strerror(errno));
			return false;
		}

		return true;
	}

	static bool isDisconnectErrno(int e)
	{
		return e == EIO || e == ENODEV || e == EBADF || e == EPIPE || e == ENXIO;
	}

private:
	std::string m_device;
	speed_t m_baud;
	int fd;
	std::string m_lastError;
};
