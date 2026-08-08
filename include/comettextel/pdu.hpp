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
     * @param bytes The bytes to convert.
     * @return The uppercase hex string.
     */
    [[nodiscard]] static std::string bytes_to_hex(std::span<const std::uint8_t> bytes);

    /**
     * @brief Parses an uppercase/lowercase hex string into bytes.
     * @param hex The hex string to parse.
     * @param out The destination bytes.
     * @return Empty error_code on success.
     */
    [[nodiscard]] static std::error_code hex_to_bytes(std::string_view hex, std::vector<std::uint8_t>& out);

    /**
     * @brief Packs septets into the GSM 7-bit packing format.
     * @param text The text to encode.
     * @return The encoded 7-bit bytes.
     */
    [[nodiscard]] static std::vector<std::uint8_t> encode_7bit(std::string_view text);

    /**
     * @brief Unpacks GSM 7-bit packed data into a 7-bit character string.
     * @param packed Packed octets.
     * @param septet_count Number of septets described by TP-UDL.
     * @return The decoded 7-bit string.
     */
    [[nodiscard]] static std::string decode_7bit(std::span<const std::uint8_t> packed,
                                                 std::size_t septet_count);

    /**
     * @brief Encodes text as UCS-2 (UTF-16BE) octets.
     * @param utf8 The UTF-8 string to encode.
     * @param out The destination bytes.
     * @return Empty error_code on success.
     */
    [[nodiscard]] static std::error_code encode_ucs2(std::string_view utf8,
                                                     std::vector<std::uint8_t>& out);

    /**
     * @brief Decodes UCS-2 (UTF-16BE) octets into a UTF-8 string.
     * @param bytes The bytes to decode.
     * @param out The destination string.
     * @return Empty error_code on success.
     */
    [[nodiscard]] static std::error_code decode_ucs2(std::span<const std::uint8_t> bytes,
                                                     std::string& out);

    /**
     * @brief Swaps digit pairs for GSM semi-octet address fields, padding with 'F'.
     * @param digits The digits to invert.
     * @return The inverted digits.
     */
    [[nodiscard]] static std::string invert_digits(std::string_view digits);

    /**
     * @brief Restores normal digit order from a semi-octet hex field.
     * @param inverted The inverted digits.
     * @return The serialized digits.
     */
    [[nodiscard]] static std::string serialize_digits(std::string_view inverted);

    /**
     * @brief Encodes a submit-PDU hex string for @c AT+CMGS.
     * @param message Message parameters (SMSC, destination, coding, text).
     * @param pdu_hex Receives the hex PDU (without the trailing Ctrl-Z).
     * @return Empty error_code on success.
     *
     * @note Single-segment only (no UDH / concatenated SMS). Length limits:
     *       GSM 7-bit ≤ 160 septets; 8-bit / UCS-2 ≤ 140 octets.
     */
    [[nodiscard]] static std::error_code encode(const Message& message, std::string& pdu_hex);

    /**
     * @brief Decodes a PDU hex string into @ref Message fields.
     *
     * Supports SMS-DELIVER (receive) and SMS-SUBMIT (as produced by @ref encode)
     * based on the TP-MTI bits in the first TPDU octet.
     *
     * @note User-Data Header (UDH): when TP-UDHI is set, the header octets are
     *       skipped so @ref Message::user_data contains the payload text only.
     *       Concatenated segments are still not reassembled.
     */
    [[nodiscard]] static std::error_code decode(std::string_view pdu_hex, Message& message);
};

} // namespace comettextel
