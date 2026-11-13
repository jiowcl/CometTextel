/**
 * @file tpdu_limits.hpp
 * @brief SMS-SUBMIT segmentation limits and concat UDH descriptor (internal).
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace comettextel::detail::tpdu {

inline constexpr std::size_t kGsm7SingleLimit = 160;
inline constexpr std::size_t kOctetSingleLimit = 140;
inline constexpr std::size_t kConcatUdhOctets = 6; // UDHL + IEI 0x00 (8-bit ref)
inline constexpr std::size_t kGsm7ConcatSkipSeptets = (kConcatUdhOctets * 8U + 6U) / 7U; // 7
inline constexpr std::size_t kGsm7ConcatPayload = kGsm7SingleLimit - kGsm7ConcatSkipSeptets; // 153
inline constexpr std::size_t kOctetConcatPayload = kOctetSingleLimit - kConcatUdhOctets; // 134
inline constexpr std::size_t kMaxConcatSegments = 255;

/**
 * @brief Concat UDH descriptor.
 * @param ref The reference number.
 * @param total The total number of segments.
 * @param seq The sequence number.
 */
struct ConcatUdh {
    std::uint8_t ref{};
    std::uint8_t total{};
    std::uint8_t seq{};
};

} // namespace comettextel::detail::tpdu
