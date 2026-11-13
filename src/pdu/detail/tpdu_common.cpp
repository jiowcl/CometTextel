/**
 * @file tpdu_common.cpp
 * @brief TPDU address / UDH helpers (internal).
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#include "pdu/detail/tpdu_common.hpp"

#include <cctype>

namespace comettextel::detail::tpdu {

/**
 * @brief Normalizes a phone number digits.
 * @param address The phone number to normalize.
 * @return The normalized phone number.
 */
std::string normalize_address_digits(std::string_view address)
{
    std::string digits;
    digits.reserve(address.size());

    for (char ch : address) {
        if (ch == '+') {
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(ch))) {
            digits.push_back(ch);
        }
    }

    return digits;
}

/**
 * @brief Parses a concat UDH descriptor from a UDH.
 * @param udh The UDH to parse.
 * @param message The message to parse to.
 */
void parse_concat_from_udh(std::span<const std::uint8_t> udh, Message& message)
{
    if (udh.empty()) {
        return;
    }

    const std::size_t udhl = udh[0];
    const std::size_t end = 1U + udhl;

    if (udh.size() < end) {
        return;
    }

    std::size_t i = 1;

    while (i + 2U <= end) {
        const std::uint8_t iei = udh[i++];
        const std::uint8_t iedl = udh[i++];

        if (i + static_cast<std::size_t>(iedl) > end) {
            break;
        }

        if (iei == 0x00U && iedl == 0x03U) {
            message.is_concatenated = true;
            message.concat_ref = udh[i];
            message.concat_total = udh[i + 1U];
            message.concat_seq = udh[i + 2U];
            return;
        }

        if (iei == 0x08U && iedl == 0x04U) {
            message.is_concatenated = true;
            message.concat_ref = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(udh[i]) << 8) | udh[i + 1U]);
            message.concat_total = udh[i + 2U];
            message.concat_seq = udh[i + 3U];
            return;
        }

        i += iedl;
    }
}

} // namespace comettextel::detail::tpdu
