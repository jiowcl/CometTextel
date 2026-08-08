/**
 * @file types.hpp
 * @brief Common types, enums, and error codes for the CometTextel library.
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @email jiowcl@gmail.com
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#pragma once

#include <cstdint>
#include <string>
#include <system_error>

#include "comettextel/export.hpp"

namespace comettextel {

/**
 * @brief Alphabet used for the TP-User-Data field (TP-DCS).
 */
enum class DataCoding : std::uint8_t {
    Gsm7Bit = 0,   ///< GSM 7-bit default alphabet
    EightBit = 4,  ///< 8-bit data
    Ucs2 = 8       ///< UCS-2 (UTF-16BE) text
};

/**
 * @brief Result of waiting for a modem textual response.
 */
enum class ModemResponse : std::int8_t {
    Wait = 0,   ///< More data may still arrive
    Ok = 1,     ///< Final OK received
    Error = -1  ///< CMS/CME or generic ERROR
};

/**
 * @brief Library-specific error conditions.
 */
enum class Errc : int {
    Ok = 0,
    InvalidArgument,
    NotOpen,
    AlreadyOpen,
    IoFailure,
    Timeout,
    ModemRejected,
    EncodeFailure,
    DecodeFailure,
    Unsupported
};

/**
 * @brief Error category for @ref Errc.
 * @return The error category.
 */
class ErrorCategory final : public std::error_category {
public:
    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] std::string message(int condition) const override;
};

/**
 * @brief Returns the singleton error category for this library.
 * @return The error category.
 */
[[nodiscard]] COMETTEXTEL_API const std::error_category& error_category() noexcept;

/**
 * @brief Creates an error_code from @ref Errc.
 * @param e The error code.
 * @return The error code.
 */
[[nodiscard]] inline std::error_code make_error_code(Errc e) noexcept
{
    return {static_cast<int>(e), error_category()};
}

/**
 * @brief Short message parameters shared by PDU encode and decode paths.
 */
struct Message {
    std::string service_center; ///< SMSC address digits (SCA), optional leading '+'
    std::string peer_address;   ///< Destination (TP-DA) or originator (TP-RA)
    std::uint8_t protocol_id{0}; ///< TP-PID
    DataCoding coding{DataCoding::Ucs2}; ///< TP-DCS
    std::string service_timestamp; ///< TP-SCTS (receive path)
    std::string user_data; ///< Decoded TP-UD text / bytes as characters (UDH stripped when present)
    std::int16_t index{-1}; ///< Storage index when listing messages
    bool has_udh{false}; ///< True when TP-UDHI was set (header skipped; not reassembled)
};

/**
 * @brief Accumulator for fragmented modem replies.
 */
struct ResponseBuffer {
    std::string data; ///< Raw bytes received so far
};

} // namespace comettextel

namespace std {
template <>
struct is_error_code_enum<comettextel::Errc> : true_type {};
} // namespace std
