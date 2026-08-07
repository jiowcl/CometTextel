/**
 * @file serial.hpp
 * @brief Cross-platform serial port abstraction.
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @email jiowcl@gmail.com
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "comettextel/export.hpp"

namespace comettextel {

/**
 * @brief Serial parity mode.
 */
enum class Parity {
    None, ///< No parity
    Odd,  ///< Odd parity
    Even, ///< Even parity
    Mark, ///< Mark parity (platform-dependent)
    Space ///< Space parity (platform-dependent)
};

/**
 * @brief Serial stop-bit configuration.
 */
enum class StopBits {
    One,          ///< 1 stop bit
    OnePointFive, ///< 1.5 stop bits
    Two           ///< 2 stop bits
};

/**
 * @brief Parameters used when opening a serial device.
 */
struct SerialConfig {
    std::uint32_t baud_rate{57600}; ///< Baud rate in bits per second
    std::uint8_t data_bits{8};      ///< Data bits (typically 7 or 8)
    Parity parity{Parity::None};    ///< Parity mode
    StopBits stop_bits{StopBits::One}; ///< Stop bits

    std::chrono::milliseconds read_interval_timeout{100}; ///< Max idle gap between bytes
    std::chrono::milliseconds read_timeout{500};          ///< Total read budget
    std::chrono::milliseconds write_timeout{100};         ///< Total write budget
};

/**
 * @brief RAII serial port with platform backends (Win32 / POSIX).
 *
 * The public API is identical on every platform. Platform-specific code lives
 * behind @ref Impl and is selected at build time by CMake.
 */
class COMETTEXTEL_API SerialPort {
public:
    SerialPort();
    ~SerialPort();

    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    SerialPort(SerialPort&&) noexcept;
    SerialPort& operator=(SerialPort&&) noexcept;

    /**
     * @brief Opens a serial device.
     * @param device Device path, e.g. "COM3", "\\\\.\\COM10", or "/dev/ttyUSB0".
     * @param config Port configuration.
     * @return Empty error_code on success.
     */
    [[nodiscard]] std::error_code open(std::string_view device, const SerialConfig& config = {});

    /**
     * @brief Closes the port if it is open.
     */
    void close() noexcept;

    /**
     * @brief Returns whether the port currently holds an open handle.
     */
    [[nodiscard]] bool is_open() const noexcept;

    /**
     * @brief Writes bytes to the port.
     * @param data Bytes to transmit.
     * @param written Optional out-parameter for the number of bytes written.
     * @return Empty error_code on success.
     */
    [[nodiscard]] std::error_code write(std::span<const std::byte> data, std::size_t* written = nullptr);

    /**
     * @brief Convenience overload for character buffers.
     */
    [[nodiscard]] std::error_code write(std::string_view data, std::size_t* written = nullptr);

    /**
     * @brief Reads up to @p max_bytes from the port.
     * @param max_bytes Maximum number of bytes to read.
     * @param out Receives the data that was read (may be empty on timeout).
     * @return Empty error_code on success (including a soft timeout with empty @p out).
     */
    [[nodiscard]] std::error_code read(std::size_t max_bytes, std::vector<std::byte>& out);

    /**
     * @brief Reads into an existing buffer and returns the number of bytes read.
     */
    [[nodiscard]] std::error_code read(std::span<std::byte> buffer, std::size_t& read_count);

    /**
     * @brief Returns the device path used in the last successful @ref open.
     */
    [[nodiscard]] const std::string& device() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace comettextel
