/**
 * @file modem.cpp
 * @brief GSM modem lifecycle and generic AT (I/O: modem_at_io, URC: modem_urc, SMS: modem_messages).
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @email jiowcl@gmail.com
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#include "comettextel/modem.hpp"

#include <string>

namespace comettextel {

/**
 * @brief Constructs a new GsmModem object with an owned serial port.
 */
GsmModem::GsmModem()
    : port_(&owned_port_)
    , owns_port_(true)
{
}

/**
 * @brief Constructs a new GsmModem object with a shared serial port.
 * @param port The serial port to use.
 */
GsmModem::GsmModem(SerialPort& port)
    : port_(&port)
    , owns_port_(false)
{
}

/**
 * @brief Opens and initializes the modem.
 * @param device The device to open.
 * @param config The serial configuration.
 * @return The error code.
 */
std::error_code GsmModem::open_and_init(std::string_view device, const SerialConfig& config)
{
    std::lock_guard lock(mutex_);

    if (auto ec = port_->open(device, config); ec) {
        return ec;
    }

    return initialize();
}

/**
 * @brief Initializes the modem.
 * @return The error code.
 */
std::error_code GsmModem::initialize()
{
    std::lock_guard lock(mutex_);

    if (!port_->is_open()) {
        return make_error_code(Errc::NotOpen);
    }

    ResponseBuffer buffer;

    if (auto ec = write_string("AT\r"); ec) {
        return ec;
    }

    buffer.data.clear();
    if (auto ec = wait_until_ok_unlocked(buffer, std::chrono::seconds(3),
                                         std::chrono::milliseconds(50));
        ec) {
        return ec;
    }

    if (auto ec = write_string("ATE0\r"); ec) {
        return ec;
    }

    buffer.data.clear();
    if (auto ec = wait_until_ok_unlocked(buffer, std::chrono::seconds(3),
                                         std::chrono::milliseconds(50));
        ec) {
        return ec;
    }

    if (auto ec = write_string("AT+CMGF=0\r"); ec) {
        return ec;
    }

    buffer.data.clear();
    if (auto ec = wait_until_ok_unlocked(buffer, std::chrono::seconds(3),
                                         std::chrono::milliseconds(50));
        ec) {
        return ec;
    }

    configure_status_report_urc();
    return {};
}

/**
 * @brief Runs an AT command.
 * @param command The command to run.
 * @param response The response buffer.
 * @param timeout The timeout duration.
 * @return The error code.
 */
std::error_code GsmModem::run_at_command(std::string_view command,
                                         ResponseBuffer& response,
                                         std::chrono::milliseconds timeout)
{
    std::lock_guard lock(mutex_);

    if (!port_->is_open()) {
        return make_error_code(Errc::NotOpen);
    }

    std::string cmd(command);
    if (cmd.empty() || (cmd.back() != '\r' && cmd.back() != '\n')) {
        cmd.push_back('\r');
    }

    response.data.clear();
    if (auto ec = write_string(cmd); ec) {
        return ec;
    }

    return wait_until_ok_unlocked(response, timeout, std::chrono::milliseconds(50));
}

/**
 * @brief Gets the serial port.
 * @return The serial port.
 */
SerialPort& GsmModem::port() noexcept
{
    return *port_;
}

/**
 * @brief Gets the serial port.
 * @return The serial port.
 */
const SerialPort& GsmModem::port() const noexcept
{
    return *port_;
}

} // namespace comettextel
