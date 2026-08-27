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
     * @brief Packs GSM septet values into the GSM 7-bit packing format.
     * @param septets One byte per septet (low 7 bits); not UTF-8 text.
     * @return The packed octets.
     *
     * @note For UTF-8 message text use @ref utf8_to_gsm7 then this, or
     *       @ref encode / @ref encode_segments with @ref DataCoding::Gsm7Bit.
     */
    [[nodiscard]] static std::vector<std::uint8_t> encode_7bit(std::string_view septets);

    /**
     * @brief Unpacks GSM 7-bit packed data into raw septet values.
     * @param packed Packed octets.
     * @param septet_count Number of septets described by TP-UDL.
     * @return One byte per septet (not UTF-8).
     *
     * @note Convert to UTF-8 with @ref gsm7_to_utf8.
     */
    [[nodiscard]] static std::string decode_7bit(std::span<const std::uint8_t> packed,
                                                 std::size_t septet_count);

    /**
     * @brief Maps UTF-8 text to GSM 03.38 default-alphabet septets (with ESC extension).
     * @param utf8 UTF-8 input.
     * @param septets Receives one byte per septet (ESC sequences use two).
     * @return Empty on success; @ref Errc::EncodeFailure if a character is not in
     *         the default alphabet or extension table.
     */
    [[nodiscard]] static std::error_code utf8_to_gsm7(std::string_view utf8, std::string& septets);

    /**
     * @brief Maps GSM 03.38 septets (including ESC pairs) to UTF-8 text.
     * @param septets Raw septet bytes from @ref decode_7bit.
     * @param utf8 Receives UTF-8 output.
     * @return Empty on success.
     */
    [[nodiscard]] static std::error_code gsm7_to_utf8(std::string_view septets, std::string& utf8);

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
     * @brief Encodes a single submit-PDU hex string for @c AT+CMGS.
     * @param message Message parameters (SMSC, destination, coding, text).
     * @param pdu_hex Receives the hex PDU (without the trailing Ctrl-Z).
     * @return Empty error_code on success.
     *
     * @note Single-segment only (no UDH). Length limits:
     *       GSM 7-bit ≤ 160 septets (after GSM 03.38 mapping; ESC chars count as 2);
     *       8-bit / UCS-2 ≤ 140 octets.
     *       Use @ref encode_segments to auto-split longer payloads.
     *       For GSM 7-bit, @ref Message::user_data is UTF-8.
     *       By default no TP-VP is emitted; set @ref Message::relative_validity_period
     *       to add a GSM relative validity period and @ref Message::request_status_report
     *       to set TP-SRR.
     */
    [[nodiscard]] static std::error_code encode(const Message& message, std::string& pdu_hex);

    /**
     * @brief Encodes one or more submit-PDU hex strings, splitting with concat UDH
     *        (IEI 0x00, 8-bit reference) when the payload exceeds a single segment.
     * @param message Message parameters (SMSC, destination, coding, text).
     * @param pdu_hexes Receives one hex PDU per segment (without Ctrl-Z).
     * @return Empty error_code on success.
     *
     * @note Fits in one segment → same as @ref encode (no UDH).
     *       Longer text → every segment carries a concat UDH. At most 255 parts.
     *       @ref Message::concat_ref is used when non-zero (low 8 bits); otherwise
     *       a checksum of the payload is used.
     *       Per-segment payload with UDH: GSM 7-bit ≤ 153 septets; 8-bit / UCS-2 ≤ 134 octets.
     *       GSM 7-bit segment splits never break an ESC + extension septet pair.
     *       TP-VP and TP-SRR options are copied to every generated segment.
     */
    [[nodiscard]] static std::error_code encode_segments(const Message& message,
                                                         std::vector<std::string>& pdu_hexes);

    /**
     * @brief Decodes a PDU hex string into @ref Message fields.
     *
     * Supports SMS-DELIVER, SMS-SUBMIT (as produced by @ref encode), and
     * SMS-STATUS-REPORT based on the TP-MTI bits in the first TPDU octet.
     *
     * @note User-Data Header (UDH): when TP-UDHI is set, the header octets are
     *       skipped so @ref Message::user_data contains the payload text only.
     *       Concatenated SMS IEI 0x00 (8-bit ref) and 0x08 (16-bit ref) are
     *       parsed into @ref Message::is_concatenated / concat_* fields.
     *       A status report sets @ref Message::is_status_report and exposes
     *       TP-MR, TP-Status, TP-RA, TP-SCTS, and TP-DT.
     *       Use @ref reassemble_messages to join complete segment sets.
     */
    [[nodiscard]] static std::error_code decode(std::string_view pdu_hex, Message& message);

    /**
     * @brief Joins complete concatenated-SMS segment sets into single messages.
     * @param messages Decoded segments and/or single-part messages (order preserved
     *        for non-grouped items and for the first segment of each group).
     * @return Messages with complete groups merged; incomplete groups left as
     *         individual segments; non-concat messages unchanged.
     *
     * @note Group key: peer address + concat_ref + concat_total + coding.
     *       A merged message has @ref Message::concat_seq == 0,
     *       @ref Message::is_concatenated == true, @c has_udh == false, and
     *       @c user_data equal to the concatenation of parts 1..total.
     *       @c index is the minimum non-negative storage index among parts
     *       (or @c -1 if none).
     */
    [[nodiscard]] static std::vector<Message> reassemble_messages(std::vector<Message> messages);
};

} // namespace comettextel
