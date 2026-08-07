/**
 * @file pdu.hpp
 * @brief GSM 03.40 PDU encode/decode helpers.
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @email jiowcl@gmail.com
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "comettextel/export.hpp"
#include "comettextel/types.hpp"

namespace comettextel {

/**
 * @brief Stateless PDU utilities (hex framing, 7-bit/UCS2 codecs, full TPDU).
 */
class COMETTEXTEL_API PduCodec {
public:
    PduCodec() = delete;

    /**
     * @brief Converts a binary buffer to an uppercase hex string.
     */
    [[nodiscard]] static std::string bytes_to_hex(std::span<const std::uint8_t> bytes);

    /**
     * @brief Parses an uppercase/lowercase hex string into bytes.
     */
    [[nodiscard]] static std::error_code hex_to_bytes(std::string_view hex, std::vector<std::uint8_t>& out);

    /**
     * @brief Packs septets into the GSM 7-bit packing format.
     */
    [[nodiscard]] static std::vector<std::uint8_t> encode_7bit(std::string_view text);

    /**
     * @brief Unpacks GSM 7-bit packed data into a 7-bit character string.
     * @param packed Packed octets.
     * @param septet_count Number of septets described by TP-UDL.
     */
    [[nodiscard]] static std::string decode_7bit(std::span<const std::uint8_t> packed,
                                                 std::size_t septet_count);

    /**
     * @brief Encodes text as UCS-2 (UTF-16BE) octets.
     */
    [[nodiscard]] static std::error_code encode_ucs2(std::string_view utf8,
                                                     std::vector<std::uint8_t>& out);

    /**
     * @brief Decodes UCS-2 (UTF-16BE) octets into a UTF-8 string.
     */
    [[nodiscard]] static std::error_code decode_ucs2(std::span<const std::uint8_t> bytes,
                                                     std::string& out);

    /**
     * @brief Swaps digit pairs for GSM semi-octet address fields, padding with 'F'.
     */
    [[nodiscard]] static std::string invert_digits(std::string_view digits);

    /**
     * @brief Restores normal digit order from a semi-octet hex field.
     */
    [[nodiscard]] static std::string serialize_digits(std::string_view inverted);

    /**
     * @brief Encodes a submit-PDU hex string for @c AT+CMGS.
     * @param message Message parameters (SMSC, destination, coding, text).
     * @param pdu_hex Receives the hex PDU (without the trailing Ctrl-Z).
     * @return Empty error_code on success.
     */
    [[nodiscard]] static std::error_code encode(const Message& message, std::string& pdu_hex);

    /**
     * @brief Decodes a deliver-PDU hex string into @ref Message fields.
     */
    [[nodiscard]] static std::error_code decode(std::string_view pdu_hex, Message& message);
};

} // namespace comettextel
