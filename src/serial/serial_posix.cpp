/**
 * @file serial_posix.cpp
 * @brief POSIX serial port backend (Linux, macOS, BSD).
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @email jiowcl@gmail.com
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#include "comettextel/serial.hpp"
#include "comettextel/types.hpp"

#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <utility>

namespace comettextel {
namespace {

/**
 * @brief Convert the baud rate to the POSIX format.
 * @param baud The baud rate.
 * @return The POSIX format of the baud rate.
 */
[[nodiscard]] speed_t to_posix_baud(std::uint32_t baud)
{
    switch (baud) {
    case 9600:
        return B9600;
    case 19200:
        return B19200;
    case 38400:
        return B38400;
    case 57600:
        return B57600;
    case 115200:
        return B115200;
    case 230400:
        return B230400;
    default:
        return 0;
    }
}

} // namespace

/**
 * @brief Implementation of the serial port.
 */
struct SerialPort::Impl {
    int fd{-1};
    std::string device;
};

/**
 * @brief Construct a new SerialPort object.
 */
SerialPort::SerialPort()
    : impl_(std::make_unique<Impl>())
{
}

/**
 * @brief Destruct the serial port.
 */
SerialPort::~SerialPort()
{
    close();
}

/**
 * @brief Construct a new SerialPort object.
 * @param other The other serial port to move from.
 */
SerialPort::SerialPort(SerialPort&&) noexcept = default;

/**
 * @brief Assign a new serial port.
 * @param other The other serial port to assign from.
 * @return The assigned serial port.
 */
SerialPort& SerialPort::operator=(SerialPort&& other) noexcept
{
    if (this != &other) {
        close();
        impl_ = std::move(other.impl_);
    }

    return *this;
}


/**
 * @brief Open the serial port.
 * @param device The device to open.
 * @param config The serial configuration.
 * @return The error code.
 */
std::error_code SerialPort::open(std::string_view device, const SerialConfig& config)
{
    if (!impl_) {
        impl_ = std::make_unique<Impl>();
    }

    if (is_open()) {
        return make_error_code(Errc::AlreadyOpen);
    }

    if (device.empty()) {
        return make_error_code(Errc::InvalidArgument);
    }

    const speed_t speed = to_posix_baud(config.baud_rate);

    if (speed == 0) {
        return make_error_code(Errc::Unsupported);
    }

    if (config.data_bits < 5 || config.data_bits > 8) {
        return make_error_code(Errc::InvalidArgument);
    }

    const std::string path(device);
    const int fd = ::open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);

    if (fd < 0) {
        return std::error_code(errno, std::generic_category());
    }

    // Clear O_NONBLOCK after open so reads honor VTIME/VMIN.
    const int flags = fcntl(fd, F_GETFL, 0);

    if (flags >= 0) {
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    }

    termios tio{};

    if (tcgetattr(fd, &tio) != 0) {
        const int err = errno;
        ::close(fd);
        return std::error_code(err, std::generic_category());
    }

    cfmakeraw(&tio);
    cfsetispeed(&tio, speed);
    cfsetospeed(&tio, speed);

    tio.c_cflag |= (CLOCAL | CREAD);
    tio.c_cflag &= ~CSIZE;

    switch (config.data_bits) {
    case 5:
        tio.c_cflag |= CS5;
        break;
    case 6:
        tio.c_cflag |= CS6;
        break;
    case 7:
        tio.c_cflag |= CS7;
        break;
    default:
        tio.c_cflag |= CS8;
        break;
    }

    switch (config.parity) {
    case Parity::Odd:
        tio.c_cflag |= (PARENB | PARODD);
        break;
    case Parity::Even:
        tio.c_cflag |= PARENB;
        tio.c_cflag &= ~PARODD;
        break;
    case Parity::None:
    default:
        tio.c_cflag &= ~PARENB;
        break;
    }

    if (config.stop_bits == StopBits::Two) {
        tio.c_cflag |= CSTOPB;
    } else {
        tio.c_cflag &= ~CSTOPB;
    }

    const auto read_timeout = config.read_timeout.count();
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = static_cast<cc_t>(std::max<long long>(1, read_timeout / 100));

    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        const int err = errno;
        ::close(fd);
        return std::error_code(err, std::generic_category());
    }

    impl_->fd = fd;
    impl_->device = path;

    return {};
}


/**
 * @brief Close the serial port.
 */
void SerialPort::close() noexcept
{
    if (!impl_) {
        return;
    }

    if (impl_->fd >= 0) {
        ::close(impl_->fd);
        impl_->fd = -1;
    }

    impl_->device.clear();
}


/**
 * @brief Check if the serial port is open.
 * @return True if the serial port is open, false otherwise.
 */
bool SerialPort::is_open() const noexcept
{
    return impl_ && impl_->fd >= 0;
}

/**
 * @brief Write data to the serial port.
 * @param data The data to write.
 * @param written The number of bytes written.
 * @return The error code.
 */
std::error_code SerialPort::write(std::span<const std::byte> data, std::size_t* written)
{
    if (!is_open()) {
        return make_error_code(Errc::NotOpen);
    }

    if (data.empty()) {
        if (written) {
            *written = 0;
        }

        return {};
    }

    const ssize_t n = ::write(impl_->fd, data.data(), data.size());

    if (n < 0) {
        if (written) {
            *written = 0;
        }

        return std::error_code(errno, std::generic_category());
    }

    if (written) {
        *written = static_cast<std::size_t>(n);
    }

    return {};
}

/**
 * @brief Write data to the serial port.
 * @param data The data to write.
 * @param written The number of bytes written.
 * @return The error code.
 */
std::error_code SerialPort::write(std::string_view data, std::size_t* written)
{
    const auto* bytes = reinterpret_cast<const std::byte*>(data.data());

    return write(std::span<const std::byte>{bytes, data.size()}, written);
}


/**
 * @brief Read data from the serial port.
 * @param max_bytes The maximum number of bytes to read.
 * @param out The buffer to read the data into.
 * @return The error code.
 */
std::error_code SerialPort::read(std::size_t max_bytes, std::vector<std::byte>& out)
{
    out.clear();
    if (!is_open()) {
        return make_error_code(Errc::NotOpen);
    }
    if (max_bytes == 0) {
        return {};
    }

    out.resize(max_bytes);
    std::size_t read_count = 0;
    const auto ec = read(std::span<std::byte>{out.data(), out.size()}, read_count);
    out.resize(read_count);
    
    return ec;
}


/**
 * @brief Read data from the serial port.
 * @param buffer The buffer to read the data into.
 * @param read_count The number of bytes read.
 * @return The error code.
 */
std::error_code SerialPort::read(std::span<std::byte> buffer, std::size_t& read_count)
{
    read_count = 0;

    if (!is_open()) {
        return make_error_code(Errc::NotOpen);
    }

    if (buffer.empty()) {
        return {};
    }

    const ssize_t n = ::read(impl_->fd, buffer.data(), buffer.size());

    if (n < 0) {
        return std::error_code(errno, std::generic_category());
    }

    read_count = static_cast<std::size_t>(n);

    return {};
}


/**
 * @brief Get the device of the serial port.
 * @return The device of the serial port.
 */
const std::string& SerialPort::device() const noexcept
{
    static const std::string empty;
    
    return impl_ ? impl_->device : empty;
}

} // namespace comettextel
