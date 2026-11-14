/**
 * @file modem_init.cpp
 * @brief Open port and PDU-mode initialization (AT / ATE0 / AT+CMGF=0 / CNMI).
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#include "comettextel/modem.hpp"

#include "modem/detail/profile.hpp"

#include <chrono>

namespace comettextel {

/**
 * @brief Sets the modem profile.
 * @param profile The modem profile.
 */
void GsmModem::set_modem_profile(const ModemProfileId profile) noexcept
{
    std::lock_guard lock(mutex_);
    profile_id_ = profile;
    if (profile != ModemProfileId::Auto) {
        resolved_profile_id_ = profile;
    }
}

/**
 * @brief Gets the modem profile.
 * @return The modem profile.
 */
ModemProfileId GsmModem::modem_profile() const noexcept
{
    std::lock_guard lock(mutex_);
    return profile_id_;
}

/**
 * @brief Gets the resolved modem profile.
 * @return The resolved modem profile.
 */
ModemProfileId GsmModem::resolved_modem_profile() const noexcept
{
    std::lock_guard lock(mutex_);
    return resolved_profile_id_;
}

/**
 * @brief Gets the modem manufacturer.
 * @return The modem manufacturer.
 */
std::string_view GsmModem::modem_manufacturer() const noexcept
{
    std::lock_guard lock(mutex_);
    return modem_manufacturer_;
}

/**
 * @brief Gets the modem model.
 * @return The modem model.
 */
std::string_view GsmModem::modem_model() const noexcept
{
    std::lock_guard lock(mutex_);
    return modem_model_;
}

/**
 * @brief Resolves the modem profile.
 */
void GsmModem::resolve_modem_profile_unlocked()
{
    modem_manufacturer_.clear();
    modem_model_.clear();

    if (profile_id_ != ModemProfileId::Auto) {
        resolved_profile_id_ = profile_id_;
        return;
    }

    ResponseBuffer buffer;

    if (write_string("AT+CGMI\r")) {
        resolved_profile_id_ = ModemProfileId::Generic;
        return;
    }

    buffer.data.clear();
    if (wait_until_ok_unlocked(buffer, std::chrono::seconds(3), std::chrono::milliseconds(50))) {
        resolved_profile_id_ = ModemProfileId::Generic;
        return;
    }

    modem_manufacturer_ = std::string{
        detail::modem::first_response_line(buffer.data)};

    buffer.data.clear();
    if (write_string("AT+CGMM\r")) {
        resolved_profile_id_ = ModemProfileId::Generic;
        return;
    }

    if (wait_until_ok_unlocked(buffer, std::chrono::seconds(3), std::chrono::milliseconds(50))) {
        resolved_profile_id_ = ModemProfileId::Generic;
        return;
    }

    modem_model_ = std::string{detail::modem::first_response_line(buffer.data)};
    resolved_profile_id_ =
        detail::modem::detect_profile(modem_manufacturer_, modem_model_);
}

/**
 * @brief Runs the profile post-initialization.
 */
void GsmModem::run_profile_post_init_unlocked()
{
    const auto& profile = detail::modem::profile_for(resolved_profile_id_);
    for (std::size_t i = 0; i < profile.post_init_command_count; ++i) {
        ResponseBuffer buffer;
        if (write_string(profile.post_init_commands[i])) {
            continue;
        }
        buffer.data.clear();
        (void)wait_until_ok_unlocked(buffer, std::chrono::seconds(3),
                                     std::chrono::milliseconds(50));
    }
}

/**
 * @brief Configures the message storage.
 */
void GsmModem::configure_message_storage_unlocked()
{
    const auto& profile = detail::modem::profile_for(resolved_profile_id_);

    for (std::size_t i = 0; i < profile.cpms_command_count; ++i) {
        ResponseBuffer buffer;
        const char* cmd = profile.cpms_commands[i];
        if (write_string(cmd)) {
            continue;
        }
        buffer.data.clear();
        if (!wait_until_ok_unlocked(buffer, std::chrono::seconds(3),
                                     std::chrono::milliseconds(50))) {
            return;
        }
    }
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

    resolve_modem_profile_unlocked();
    run_profile_post_init_unlocked();
    configure_message_storage_unlocked();
    configure_status_report_urc();
    return {};
}

} // namespace comettextel
