/**
 * @file at_response.hpp
 * @brief AT final-result classification helpers (internal).
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#pragma once

#include "comettextel/types.hpp"

#include <string_view>

namespace comettextel::detail::modem::at {

/**
 * @brief Checks if a string ends with a suffix.
 * @param text The string to check.
 * @param suffix The suffix to check for.
 * @return True if the string ends with the suffix, false otherwise.
 */
[[nodiscard]] bool ends_with(std::string_view text, std::string_view suffix);

/**
 * @brief Checks if a string contains a token.
 * @param text The string to check.
 * @param token The token to check for.
 * @return True if the string contains the token, false otherwise.
 */
[[nodiscard]] bool contains_token(std::string_view text, std::string_view token);

/**
 * @brief Classifies accumulated modem text (no serial I/O).
 */
[[nodiscard]] ModemResponse classify_response(std::string_view data);

} // namespace comettextel::detail::modem::at
