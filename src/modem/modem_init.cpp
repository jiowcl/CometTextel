/**
 * @file modem_init.cpp
 * @brief Open port and PDU-mode initialization (AT / ATE0 / AT+CMGF=0 / CNMI).
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#include "comettextel/modem.hpp"

#include <chrono>

namespace comettextel {

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

} // namespace comettextel
