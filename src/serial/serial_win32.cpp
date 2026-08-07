/**
 * @file serial_win32.cpp
 * @brief Win32 serial port backend.
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @email jiowcl@gmail.com
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#include "comettextel/serial.hpp"
#include "comettextel/types.hpp"

#ifndef NOMINMAX
#  define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <utility>

namespace comettextel {
namespace {

/**
 * @brief Convert the parity to the Windows format.
 * @param parity The parity.
 * @return The Windows format of the parity.
 */
[[nodiscard]] BYTE to_win_parity(Parity parity)
{
    switch (parity) {
    case Parity::Odd:
        return ODDPARITY;
    case Parity::Even:
        return EVENPARITY;
    case Parity::Mark:
        return MARKPARITY;
    case Parity::Space:
        return SPACEPARITY;
    case Parity::None:
    default:
        return NOPARITY;
    }
}

/**
 * @brief Convert the stop bits to the Windows format.
 * @param stop_bits The stop bits.
 * @return The Windows format of the stop bits.
 */
[[nodiscard]] BYTE to_win_stop_bits(StopBits stop_bits)
{
    switch (stop_bits) {
    case StopBits::OnePointFive:
        return ONE5STOPBITS;
    case StopBits::Two:
        return TWOSTOPBITS;
    case StopBits::One:
    default:
        return ONESTOPBIT;
    }
}

/**
 * @brief Normalize the Windows device name.
 * @param device The device name to normalize.
 * @return The normalized device name.
 */
[[nodiscard]] std::string normalize_win_device(std::string_view device)
{
    std::string path(device);

    if (path.rfind(R"(\\.\)", 0) == 0 || path.rfind("//./", 0) == 0) {
        return path;
    }

    // COM10+ requires the \\.\ prefix on Windows.
    if (path.rfind("COM", 0) == 0 || path.rfind("com", 0) == 0) {
        return std::string(R"(\\.\)") + path;
    }

    return path;
}

} // namespace

/**
 * @brief Implementation of the serial port.
 */
struct SerialPort::Impl {
    HANDLE handle{INVALID_HANDLE_VALUE};
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

    if (config.data_bits < 5 || config.data_bits > 8) {
        return make_error_code(Errc::InvalidArgument);
    }

    const std::string path = normalize_win_device(device);
    HANDLE handle = CreateFileA(
        path.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);

    if (handle == INVALID_HANDLE_VALUE) {
        return std::error_code(static_cast<int>(GetLastError()), std::system_category());
    }

    DCB dcb{};
    dcb.DCBlength = sizeof(DCB);

    if (!GetCommState(handle, &dcb)) {
        const DWORD err = GetLastError();
        CloseHandle(handle);
        return std::error_code(static_cast<int>(err), std::system_category());
    }

    dcb.BaudRate = config.baud_rate;
    dcb.ByteSize = config.data_bits;
    dcb.Parity = to_win_parity(config.parity);
    dcb.StopBits = to_win_stop_bits(config.stop_bits);
    dcb.fBinary = TRUE;
    dcb.fParity = (config.parity != Parity::None) ? TRUE : FALSE;

    if (!SetCommState(handle, &dcb)) {
        const DWORD err = GetLastError();
        CloseHandle(handle);
        return std::error_code(static_cast<int>(err), std::system_category());
    }

    if (!SetupComm(handle, 4096, 1024)) {
        const DWORD err = GetLastError();
        CloseHandle(handle);
        return std::error_code(static_cast<int>(err), std::system_category());
    }

    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout = static_cast<DWORD>(config.read_interval_timeout.count());
    timeouts.ReadTotalTimeoutMultiplier = 1;
    timeouts.ReadTotalTimeoutConstant = static_cast<DWORD>(config.read_timeout.count());
    timeouts.WriteTotalTimeoutMultiplier = 1;
    timeouts.WriteTotalTimeoutConstant = static_cast<DWORD>(config.write_timeout.count());

    if (!SetCommTimeouts(handle, &timeouts)) {
        const DWORD err = GetLastError();
        CloseHandle(handle);
        return std::error_code(static_cast<int>(err), std::system_category());
    }

    impl_->handle = handle;
    impl_->device.assign(device);

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

    if (impl_->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(impl_->handle);
        impl_->handle = INVALID_HANDLE_VALUE;
    }

    impl_->device.clear();
}

/**
 * @brief Check if the serial port is open.
 * @return True if the serial port is open, false otherwise.
 */
bool SerialPort::is_open() const noexcept
{
    return impl_ && impl_->handle != INVALID_HANDLE_VALUE;
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

    DWORD dw_written = 0;
    const BOOL ok = WriteFile(
        impl_->handle,
        data.data(),
        static_cast<DWORD>(data.size()),
        &dw_written,
        nullptr);

    if (written) {
        *written = static_cast<std::size_t>(dw_written);
    }

    if (!ok) {
        return std::error_code(static_cast<int>(GetLastError()), std::system_category());
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

    DWORD dw_read = 0;
    const BOOL ok = ReadFile(
        impl_->handle,
        buffer.data(),
        static_cast<DWORD>(buffer.size()),
        &dw_read,
        nullptr);

    read_count = static_cast<std::size_t>(dw_read);

    if (!ok) {
        return std::error_code(static_cast<int>(GetLastError()), std::system_category());
    }

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
