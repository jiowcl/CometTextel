/**
 * @file gsm7.hpp
 * @brief Internal GSM 03.38 helpers (concat chunk boundaries).
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#pragma once

#include <cstddef>
#include <string_view>

namespace comettextel::detail::gsm7 {

/**
 * @brief End index for a GSM-7 septet chunk that does not split ESC pairs.
 * @param septets The string to check.
 * @param start The start index to check.
 * @param max_septets The maximum number of septets to check.
 * @return The end index.
 */
[[nodiscard]] std::size_t chunk_end(std::string_view septets,
                                    std::size_t start,
                                    std::size_t max_septets) noexcept;

} // namespace comettextel::detail::gsm7
