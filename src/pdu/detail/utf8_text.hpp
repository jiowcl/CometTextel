/**
 * @file utf8_text.hpp
 * @brief Internal UTF-8 scalar helpers (PDU text pipelines).
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace comettextel::detail {

/**
 * @brief Advances the index to the next UTF-8 codepoint in a string.
 * @param text The string to advance in.
 * @param index The index to advance.
 * @param codepoint The codepoint to advance to.
 * @return True if the index was advanced, false otherwise.
 */
[[nodiscard]] bool utf8_next(std::string_view text, std::size_t& index, char32_t& codepoint);

/**
 * @brief Appends a UTF-8 codepoint to a string.
 * @param out The string to append to.
 * @param codepoint The codepoint to append.
 */
void append_utf8(std::string& out, char32_t codepoint);

} // namespace comettextel::detail
