/**
 * @file example_common.hpp
 * @brief Shared helpers for CometTextel example programs.
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @email jiowcl@gmail.com
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#pragma once

#include <chrono>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

#include "comettextel/modem.hpp"

namespace example {

[[nodiscard]] inline comettextel::SerialConfig default_config()
{
    comettextel::SerialConfig config;
    config.baud_rate = 115200;
    
    return config;
}

/**
 * @brief Polls the modem until OK, ERROR, or @p max_attempts is reached.
 */
[[nodiscard]] inline comettextel::ModemResponse wait_for_response(
    comettextel::GsmModem& modem,
    comettextel::ResponseBuffer& buffer,
    int max_attempts = 50)
{
    for (int i = 0; i < max_attempts; ++i) {
        const auto status = modem.poll_response(buffer);

        if (status != comettextel::ModemResponse::Wait) {
            return status;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    return comettextel::ModemResponse::Wait;
}

[[nodiscard]] inline const char* coding_name(comettextel::DataCoding coding)
{
    switch (coding) {
    case comettextel::DataCoding::Gsm7Bit:
        return "GSM-7";
    case comettextel::DataCoding::EightBit:
        return "8-bit";
    case comettextel::DataCoding::Ucs2:
        return "UCS-2";
    }

    return "unknown";
}

inline void print_message_summary(const comettextel::Message& message, bool full_body)
{
    std::cout << "index=" << message.index
              << " from=" << message.peer_address
              << " smsc=" << message.service_center
              << " coding=" << coding_name(message.coding);

    if (!message.service_timestamp.empty()) {
        std::cout << " scts=" << message.service_timestamp;
    }

    if (full_body) {
        std::cout << "\n  text=" << message.user_data << '\n';
    } else {
        constexpr std::size_t kPreview = 40;
        const std::string_view text = message.user_data;
        std::cout << " text=";

        if (text.size() <= kPreview) {
            std::cout << text;
        } else {
            std::cout << text.substr(0, kPreview) << "...";
        }

        std::cout << '\n';
    }
}

} // namespace example
