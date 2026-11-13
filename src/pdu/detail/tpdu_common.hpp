/**
 * @file tpdu_common.hpp
 * @brief Shared TPDU helpers (addresses, UDH concat IE).
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#pragma once

#include "comettextel/types.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace comettextel::detail::tpdu {

/**
 * @brief Normalizes a phone number digits.
 * @param address The phone number to normalize.
 * @return The normalized phone number.
 */
[[nodiscard]] std::string normalize_address_digits(std::string_view address);

/**
 * @brief Parses a concat UDH descriptor from a UDH.
 * @param udh The UDH to parse.
 * @param message The message to parse to.
 */
void parse_concat_from_udh(std::span<const std::uint8_t> udh, Message& message);

} // namespace comettextel::detail::tpdu
