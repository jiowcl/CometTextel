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

#include "comettextel/modem.hpp"

namespace example {

[[nodiscard]] inline comettextel::SerialConfig default_config()
{
    comettextel::SerialConfig config;
    config.baud_rate = 115200;
    return config;
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

    if (message.has_udh) {
        std::cout << " udh=yes";
    }
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
