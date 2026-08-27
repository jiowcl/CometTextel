/**
 * @file pdu.cpp
 * @brief GSM 03.40 PDU encode/decode implementation.
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @email jiowcl@gmail.com
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#include "comettextel/pdu.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

namespace comettextel {
namespace {

/**
 * @brief Convert a hexadecimal character to a nibble.
 * @param ch The hexadecimal character.
 * @return The nibble.
 */
[[nodiscard]] int hex_nibble(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }

    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }

    return -1;
}

/**
 * @brief Normalize the address digits.
 * @param address The address.
 * @return The normalized address digits.
 */
[[nodiscard]] std::string normalize_address_digits(std::string_view address)
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
 * @brief Parse concatenated-SMS IE from a UDH (UDHL + body).
 *
 * Recognizes IEI 0x00 (8-bit reference, IEDL=3) and IEI 0x08 (16-bit reference, IEDL=4).
 * The first matching IE wins. Invalid lengths stop the scan without failing decode.
 *
 * @param udh Span starting at UDHL (length = UDHL + 1).
 * @param message Message fields to update.
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

/**
 * @brief Decode the next UTF-8 character.
 * @param text The text to decode.
 * @param index The index of the current character.
 * @param codepoint The decoded character.
 * @return True if a character was decoded, false otherwise.
 */
[[nodiscard]] bool utf8_next(std::string_view text, std::size_t& index, char32_t& codepoint)
{
    if (index >= text.size()) {
        return false;
    }

    const auto c0 = static_cast<unsigned char>(text[index++]);

    if (c0 < 0x80) {
        codepoint = c0;

        return true;
    }

    if ((c0 & 0xE0) == 0xC0) {
        if (index >= text.size()) {
            return false;
        }

        const auto c1 = static_cast<unsigned char>(text[index++]);
        codepoint = (static_cast<char32_t>(c0 & 0x1F) << 6) | (c1 & 0x3F);

        return true;
    }

    if ((c0 & 0xF0) == 0xE0) {
        if (index + 1 >= text.size()) {
            return false;
        }

        const auto c1 = static_cast<unsigned char>(text[index++]);
        const auto c2 = static_cast<unsigned char>(text[index++]);
        codepoint = (static_cast<char32_t>(c0 & 0x0F) << 12) |
                    (static_cast<char32_t>(c1 & 0x3F) << 6) |
                    (c2 & 0x3F);

        return true;
    }

    if ((c0 & 0xF8) == 0xF0) {
        if (index + 2 >= text.size()) {
            return false;
        }
        const auto c1 = static_cast<unsigned char>(text[index++]);
        const auto c2 = static_cast<unsigned char>(text[index++]);
        const auto c3 = static_cast<unsigned char>(text[index++]);
        codepoint = (static_cast<char32_t>(c0 & 0x07) << 18) |
                    (static_cast<char32_t>(c1 & 0x3F) << 12) |
                    (static_cast<char32_t>(c2 & 0x3F) << 6) |
                    (c3 & 0x3F);

        return true;
    }

    return false;
}

/**
 * @brief Append a UTF-8 character to a string.
 * @param out The string to append the character to.
 * @param codepoint The character to append.
 */
void append_utf8(std::string& out, char32_t codepoint)
{
    if (codepoint < 0x80) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

constexpr std::size_t kGsm7SingleLimit = 160;
constexpr std::size_t kOctetSingleLimit = 140;
constexpr std::size_t kConcatUdhOctets = 6; // UDHL + IEI 0x00 (8-bit ref)
constexpr std::size_t kGsm7ConcatSkipSeptets = (kConcatUdhOctets * 8U + 6U) / 7U; // 7
constexpr std::size_t kGsm7ConcatPayload = kGsm7SingleLimit - kGsm7ConcatSkipSeptets; // 153
constexpr std::size_t kOctetConcatPayload = kOctetSingleLimit - kConcatUdhOctets; // 134
constexpr std::size_t kMaxConcatSegments = 255;

/**
 * @brief Concatenated UDH.
 */
struct ConcatUdh {
    std::uint8_t ref{};
    std::uint8_t total{};
    std::uint8_t seq{};
};

/**
 * @brief Make a concatenation reference number.
 * @param message The message to make the reference number for.
 * @return The concatenation reference number.
 */
[[nodiscard]] std::uint8_t make_concat_ref(const Message& message)
{
    if (message.concat_ref != 0) {
        return static_cast<std::uint8_t>(message.concat_ref & 0xFFU);
    }

    std::uint8_t sum = 0;
    for (unsigned char ch : message.user_data) {
        sum = static_cast<std::uint8_t>(sum + ch);
    }
    return sum == 0 ? std::uint8_t{1} : sum;
}

/**
 * @brief Append a packed 7-bit value to a buffer.
 * @param buf The buffer to append the value to.
 * @param septets The septets to append.
 * @param start_bit The starting bit.
 */
void append_packed_7bit_at(std::vector<std::uint8_t>& buf,
                           std::string_view septets,
                           std::size_t start_bit)
{
    const std::size_t total_bits = start_bit + septets.size() * 7U;
    const std::size_t total_bytes = (total_bits + 7U) / 8U;
    if (buf.size() < total_bytes) {
        buf.resize(total_bytes, 0);
    }

    for (std::size_t i = 0; i < septets.size(); ++i) {
        const auto value = static_cast<std::uint8_t>(septets[i] & 0x7F);
        const std::size_t bit_index = start_bit + i * 7U;
        for (int b = 0; b < 7; ++b) {
            if ((value & (1U << b)) == 0U) {
                continue;
            }
            const std::size_t pos = bit_index + static_cast<std::size_t>(b);
            buf[pos / 8U] = static_cast<std::uint8_t>(
                buf[pos / 8U] | static_cast<std::uint8_t>(1U << (pos % 8U)));
        }
    }
}

/**
 * @brief Append a submit header to a buffer.
 * @param buf The buffer to append the header to.
 * @param message The message to append the header to.
 * @param dest The destination address.
 * @param udhi True if the UDH is present.
 * @return The error code.
 */
std::error_code append_submit_header(std::vector<std::uint8_t>& buf,
                                     const Message& message,
                                     std::string_view dest,
                                     bool udhi)
{
    const std::string smsc = normalize_address_digits(message.service_center);

    if (smsc.empty()) {
        buf.push_back(0x00);
    } else {
        const auto smsc_len = static_cast<std::uint8_t>(
            ((smsc.size() & 1U) == 0 ? smsc.size() : smsc.size() + 1) / 2 + 1);
        buf.push_back(smsc_len);
        buf.push_back(0x91);
        const std::string inverted = PduCodec::invert_digits(smsc);
        std::vector<std::uint8_t> smsc_bytes;
        if (const auto ec = PduCodec::hex_to_bytes(inverted, smsc_bytes); ec) {
            return ec;
        }
        buf.insert(buf.end(), smsc_bytes.begin(), smsc_bytes.end());
    }

    // TP-VPF=00 means no TP-VP.  Relative TP-VP is selected only when the
    // caller explicitly supplies a value; 0x00 is a real 5-minute value.
    std::uint8_t first_octet = udhi ? std::uint8_t{0x41} : std::uint8_t{0x01};
    if (message.request_status_report) {
        first_octet = static_cast<std::uint8_t>(first_octet | 0x20U); // TP-SRR
    }
    if (message.relative_validity_period.has_value()) {
        first_octet = static_cast<std::uint8_t>(first_octet | 0x10U); // TP-VPF=10
    }

    buf.push_back(first_octet);
    buf.push_back(0x00);
    buf.push_back(static_cast<std::uint8_t>(dest.size()));
    buf.push_back(0x91);

    {
        const std::string inverted = PduCodec::invert_digits(dest);
        std::vector<std::uint8_t> dest_bytes;

        if (const auto ec = PduCodec::hex_to_bytes(inverted, dest_bytes); ec) {
            return ec;
        }

        buf.insert(buf.end(), dest_bytes.begin(), dest_bytes.end());
    }

    buf.push_back(message.protocol_id);
    buf.push_back(static_cast<std::uint8_t>(message.coding));
    if (message.relative_validity_period.has_value()) {
        buf.push_back(*message.relative_validity_period);
    }
    return {};
}

/**
 * @brief Append a concatenated UDH to a buffer.
 * @param buf The buffer to append the UDH to.
 * @param concat The concatenated UDH.
 */
void append_concat_udh(std::vector<std::uint8_t>& buf, const ConcatUdh& concat)
{
    buf.push_back(0x05);
    buf.push_back(0x00);
    buf.push_back(0x03);
    buf.push_back(concat.ref);
    buf.push_back(concat.total);
    buf.push_back(concat.seq);
}

/**
 * @brief Encode one segment of a PDU.
 * @param message The message to encode.
 * @param dest The destination address.
 * @param gsm7_payload The GSM-7 payload.
 * @param octet_payload The octet payload.
 * @param concat The concatenated UDH.
 * @param pdu_hex The encoded PDU hex string.
 * @return The error code.
 */
std::error_code encode_one_segment(const Message& message,
                                   std::string_view dest,
                                   std::string_view gsm7_payload,
                                   std::span<const std::uint8_t> octet_payload,
                                   const ConcatUdh* concat,
                                   std::string& pdu_hex)
{
    pdu_hex.clear();
    std::vector<std::uint8_t> buf;
    buf.reserve(256);

    if (const auto ec = append_submit_header(buf, message, dest, concat != nullptr); ec) {
        return ec;
    }

    switch (message.coding) {
    case DataCoding::Gsm7Bit: {
        if (concat == nullptr) {
            const auto packed = PduCodec::encode_7bit(gsm7_payload);
            buf.push_back(static_cast<std::uint8_t>(gsm7_payload.size()));
            buf.insert(buf.end(), packed.begin(), packed.end());
        } else {
            const std::size_t udl = kGsm7ConcatSkipSeptets + gsm7_payload.size();
            buf.push_back(static_cast<std::uint8_t>(udl));
            std::vector<std::uint8_t> ud;
            append_concat_udh(ud, *concat);
            append_packed_7bit_at(ud, gsm7_payload, kGsm7ConcatSkipSeptets * 7U);
            buf.insert(buf.end(), ud.begin(), ud.end());
        }
        break;
    }
    case DataCoding::Ucs2:
    case DataCoding::EightBit:
    default: {
        if (concat == nullptr) {
            buf.push_back(static_cast<std::uint8_t>(octet_payload.size()));
            buf.insert(buf.end(), octet_payload.begin(), octet_payload.end());
        } else {
            buf.push_back(static_cast<std::uint8_t>(kConcatUdhOctets + octet_payload.size()));
            append_concat_udh(buf, *concat);
            buf.insert(buf.end(), octet_payload.begin(), octet_payload.end());
        }
        break;
    }
    }

    pdu_hex = PduCodec::bytes_to_hex(buf);
    return {};
}

/**
 * @brief Check if a UTF-16 high surrogate.
 * @param hi The high byte.
 * @param lo The low byte.
 * @return True if the byte is a UTF-16 high surrogate, false otherwise.
 */
[[nodiscard]] bool is_utf16_high_surrogate(std::uint8_t hi, std::uint8_t lo)
{
    const char16_t unit = static_cast<char16_t>((hi << 8) | lo);
    return unit >= 0xD800 && unit <= 0xDBFF;
}

/**
 * @brief Split UCS-2 octets into chunks.
 * @param payload The payload to split.
 * @param max_octets The maximum number of octets to split into.
 * @param chunks The chunks to split the payload into.
 * @return The error code.
 */
std::error_code split_ucs2_octets(std::span<const std::uint8_t> payload,
                                  std::size_t max_octets,
                                  std::vector<std::vector<std::uint8_t>>& chunks)
{
    chunks.clear();
    if (payload.size() % 2U != 0U) {
        return make_error_code(Errc::EncodeFailure);
    }

    std::size_t offset = 0;
    while (offset < payload.size()) {
        std::size_t take = (std::min)(max_octets, payload.size() - offset);
        if ((take & 1U) != 0U) {
            --take;
        }
        if (take >= 2U && offset + take < payload.size() &&
            is_utf16_high_surrogate(payload[offset + take - 2U], payload[offset + take - 1U])) {
            take -= 2U;
        }
        if (take == 0U) {
            return make_error_code(Errc::EncodeFailure);
        }
        chunks.emplace_back(payload.begin() + static_cast<std::ptrdiff_t>(offset),
                            payload.begin() + static_cast<std::ptrdiff_t>(offset + take));
        offset += take;
    }
    return {};
}

} // namespace

/**
 * @brief Convert bytes to a hexadecimal string.
 * @param bytes The bytes to convert.
 * @return The hexadecimal string.
 */
std::string PduCodec::bytes_to_hex(std::span<const std::uint8_t> bytes)
{
    static constexpr char kTable[] = "0123456789ABCDEF";
    std::string out;
    out.resize(bytes.size() * 2);

    for (std::size_t i = 0; i < bytes.size(); ++i) {
        out[i * 2] = kTable[bytes[i] >> 4];
        out[i * 2 + 1] = kTable[bytes[i] & 0x0F];
    }

    return out;
}

/**
 * @brief Convert a hexadecimal string to bytes.
 * @param hex The hexadecimal string.
 * @param out The bytes to convert.
 * @return The error code.
 */
std::error_code PduCodec::hex_to_bytes(std::string_view hex, std::vector<std::uint8_t>& out)
{
    out.clear();

    if (hex.size() % 2 != 0) {
        return make_error_code(Errc::InvalidArgument);
    }

    out.reserve(hex.size() / 2);

    for (std::size_t i = 0; i < hex.size(); i += 2) {
        const int hi = hex_nibble(hex[i]);
        const int lo = hex_nibble(hex[i + 1]);

        if (hi < 0 || lo < 0) {
            out.clear();
            return make_error_code(Errc::InvalidArgument);
        }

        out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
    }

    return {};
}

/**
 * @brief Encode 7-bit septets to packed octets.
 * @param septets One byte per septet (low 7 bits).
 * @return The packed octets.
 */
std::vector<std::uint8_t> PduCodec::encode_7bit(std::string_view septets)
{
    std::vector<std::uint8_t> out;
    out.reserve((septets.size() * 7 + 7) / 8);

    std::uint8_t left = 0;

    for (std::size_t n_src = 0; n_src < septets.size(); ++n_src) {
        const int n_char = static_cast<int>(n_src & 7);
        const auto value = static_cast<std::uint8_t>(septets[n_src] & 0x7F);

        if (n_char == 0) {
            left = value;
        } else {
            out.push_back(static_cast<std::uint8_t>((value << (8 - n_char)) | left));
            left = static_cast<std::uint8_t>(value >> n_char);
        }
    }

    // Incomplete septet groups still need the residual bits flushed.
    if (!septets.empty() && (septets.size() & 7U) != 0U) {
        out.push_back(left);
    }

    return out;
}

/**
 * @brief Decode packed octets to 7-bit septets.
 * @param packed The bytes to decode.
 * @param septet_count The number of septets to decode.
 * @return One byte per septet.
 */
std::string PduCodec::decode_7bit(std::span<const std::uint8_t> packed, std::size_t septet_count)
{
    std::string out;
    out.reserve(septet_count);

    std::size_t n_src = 0;
    int n_byte = 0;
    std::uint8_t left = 0;

    while (n_src < packed.size() && out.size() < septet_count) {
        out.push_back(static_cast<char>(((packed[n_src] << n_byte) | left) & 0x7F));
        left = static_cast<std::uint8_t>(packed[n_src] >> (7 - n_byte));
        ++n_byte;

        if (n_byte == 7) {
            if (out.size() < septet_count) {
                out.push_back(static_cast<char>(left));
            }

            n_byte = 0;
            left = 0;
        }

        ++n_src;
    }

    if (out.size() > septet_count) {
        out.resize(septet_count);
    }

    return out;
}

namespace {

// GSM 03.38 default alphabet (b7..b1 value → Unicode). Index 0x1B is ESC (not a glyph).
constexpr char32_t kGsmBasic[128] = {
    U'@', U'\u00A3', U'$', U'\u00A5', U'\u00E8', U'\u00E9', U'\u00F9', U'\u00EC',
    U'\u00F2', U'\u00C7', U'\n', U'\u00D8', U'\u00F8', U'\r', U'\u00C5', U'\u00E5',
    U'\u0394', U'_', U'\u03A6', U'\u0393', U'\u039B', U'\u03A9', U'\u03A0', U'\u03A8',
    U'\u03A3', U'\u0398', U'\u039E', U'\uFFFD', U'\u00C6', U'\u00E6', U'\u00DF', U'\u00C9',
    U' ', U'!', U'"', U'#', U'\u00A4', U'%', U'&', U'\'',
    U'(', U')', U'*', U'+', U',', U'-', U'.', U'/',
    U'0', U'1', U'2', U'3', U'4', U'5', U'6', U'7',
    U'8', U'9', U':', U';', U'<', U'=', U'>', U'?',
    U'\u00A1', U'A', U'B', U'C', U'D', U'E', U'F', U'G',
    U'H', U'I', U'J', U'K', U'L', U'M', U'N', U'O',
    U'P', U'Q', U'R', U'S', U'T', U'U', U'V', U'W',
    U'X', U'Y', U'Z', U'\u00C4', U'\u00D6', U'\u00D1', U'\u00DC', U'\u00A7',
    U'\u00BF', U'a', U'b', U'c', U'd', U'e', U'f', U'g',
    U'h', U'i', U'j', U'k', U'l', U'm', U'n', U'o',
    U'p', U'q', U'r', U's', U't', U'u', U'v', U'w',
    U'x', U'y', U'z', U'\u00E4', U'\u00F6', U'\u00F1', U'\u00FC', U'\u00E0',
};

constexpr std::uint8_t kGsmEsc = 0x1B;

struct GsmExtEntry {
    std::uint8_t code;
    char32_t cp;
};

// GSM 03.38 default alphabet extension table (after ESC).
constexpr GsmExtEntry kGsmExt[] = {
    {0x0A, U'\f'},      // page break / form feed
    {0x14, U'^'},
    {0x28, U'{'},
    {0x29, U'}'},
    {0x2F, U'\\'},
    {0x3C, U'['},
    {0x3D, U'~'},
    {0x3E, U']'},
    {0x40, U'|'},
    {0x65, U'\u20AC'}, // euro
};

/**
 * @brief Encode a codepoint to GSM 03.38 septets.
 * @param cp The codepoint to encode.
 * @param septets The septets to encode.
 * @return True if the codepoint was encoded, false otherwise.
 */
[[nodiscard]] bool gsm7_encode_codepoint(char32_t cp, std::string& septets)
{
    for (int i = 0; i < 128; ++i) {
        if (i == static_cast<int>(kGsmEsc)) {
            continue;
        }
        if (kGsmBasic[static_cast<std::size_t>(i)] == cp) {
            septets.push_back(static_cast<char>(i));
            return true;
        }
    }
    for (const GsmExtEntry& entry : kGsmExt) {
        if (entry.cp == cp) {
            septets.push_back(static_cast<char>(kGsmEsc));
            septets.push_back(static_cast<char>(entry.code));
            return true;
        }
    }

    return false;
}

/**
 * @brief Get the codepoint for a GSM 03.38 extension code.
 * @param code The extension code.
 * @return The codepoint.
 */
[[nodiscard]] char32_t gsm7_ext_codepoint(std::uint8_t code) noexcept
{
    for (const GsmExtEntry& entry : kGsmExt) {
        if (entry.code == code) {
            return entry.cp;
        }
    }

    // Unknown extension: display the basic-table glyph for the second septet.
    return kGsmBasic[code & 0x7F];
}

/**
 * @brief End index for a GSM-7 septet chunk that does not split ESC pairs.
 * @param septets The septets to process.
 * @param start The start index.
 * @param max_septets The maximum number of septets to process.
 * @return The end index.
 */
[[nodiscard]] std::size_t gsm7_chunk_end(std::string_view septets,
                                         std::size_t start,
                                         std::size_t max_septets) noexcept
{
    if (start >= septets.size() || max_septets == 0) {
        return start;
    }

    std::size_t end = start + (std::min)(max_septets, septets.size() - start);
    if (end > start && static_cast<unsigned char>(septets[end - 1]) == kGsmEsc &&
        end < septets.size()) {
        --end;
    }

    if (end == start) {
        // Need progress: take ESC+next together when required.
        if (static_cast<unsigned char>(septets[start]) == kGsmEsc && start + 1 < septets.size()) {
            if (max_septets < 2) {
                return start; // cannot fit escape pair in this budget
            }
            return start + 2;
        }
        return start + 1;
    }

    return end;
}

} // namespace

/**
 * @brief Map UTF-8 text to GSM 03.38 septets.
 * @param utf8 UTF-8 input.
 * @param septets Receives septet bytes.
 * @return Empty on success.
 */
std::error_code PduCodec::utf8_to_gsm7(std::string_view utf8, std::string& septets)
{
    septets.clear();
    septets.reserve(utf8.size());

    std::size_t index = 0;
    char32_t codepoint = 0;

    while (index < utf8.size()) {
        if (!utf8_next(utf8, index, codepoint)) {
            septets.clear();
            return make_error_code(Errc::EncodeFailure);
        }
        if (!gsm7_encode_codepoint(codepoint, septets)) {
            septets.clear();
            return make_error_code(Errc::EncodeFailure);
        }
    }

    return {};
}

/**
 * @brief Map GSM 03.38 septets to UTF-8 text.
 * @param septets Raw septet bytes.
 * @param utf8 Receives UTF-8 output.
 * @return Empty on success.
 */
std::error_code PduCodec::gsm7_to_utf8(std::string_view septets, std::string& utf8)
{
    utf8.clear();
    utf8.reserve(septets.size());

    for (std::size_t i = 0; i < septets.size(); ++i) {
        const auto value = static_cast<std::uint8_t>(septets[i] & 0x7F);

        if (value == kGsmEsc) {
            if (i + 1 >= septets.size()) {
                append_utf8(utf8, U' '); // lone ESC → space
                break;
            }

            const auto ext = static_cast<std::uint8_t>(septets[++i] & 0x7F);

            if (ext == kGsmEsc) {
                append_utf8(utf8, U' '); // reserved second ESC
            } else {
                append_utf8(utf8, gsm7_ext_codepoint(ext));
            }
            continue;
        }

        append_utf8(utf8, kGsmBasic[value]);
    }
    
    return {};
}

/**
 * @brief Encode UTF-8 text to UCS-2 bytes.
 * @param utf8 The UTF-8 text to encode.
 * @param out The encoded bytes.
 * @return The error code.
 */
std::error_code PduCodec::encode_ucs2(std::string_view utf8, std::vector<std::uint8_t>& out)
{
    out.clear();
    std::size_t index = 0;
    char32_t codepoint = 0;

    while (index < utf8.size()) {
        if (!utf8_next(utf8, index, codepoint)) {
            out.clear();
            return make_error_code(Errc::EncodeFailure);
        }

        if (codepoint <= 0xFFFF) {
            out.push_back(static_cast<std::uint8_t>((codepoint >> 8) & 0xFF));
            out.push_back(static_cast<std::uint8_t>(codepoint & 0xFF));
        } else {
            // Encode as UTF-16 surrogate pair in big-endian order.
            const char32_t adjusted = codepoint - 0x10000;
            const char16_t high = static_cast<char16_t>(0xD800 | ((adjusted >> 10) & 0x3FF));
            const char16_t low = static_cast<char16_t>(0xDC00 | (adjusted & 0x3FF));

            out.push_back(static_cast<std::uint8_t>((high >> 8) & 0xFF));
            out.push_back(static_cast<std::uint8_t>(high & 0xFF));
            out.push_back(static_cast<std::uint8_t>((low >> 8) & 0xFF));
            out.push_back(static_cast<std::uint8_t>(low & 0xFF));
        }
    }

    return {};
}

/**
 * @brief Decode UCS-2 bytes to UTF-8 text.
 * @param bytes The bytes to decode.
 * @param out The decoded text.
 * @return The error code.
 */
std::error_code PduCodec::decode_ucs2(std::span<const std::uint8_t> bytes, std::string& out)
{
    out.clear();

    if (bytes.size() % 2 != 0) {
        return make_error_code(Errc::DecodeFailure);
    }

    for (std::size_t i = 0; i < bytes.size();) {
        const char16_t unit = static_cast<char16_t>((bytes[i] << 8) | bytes[i + 1]);
        i += 2;

        if (unit >= 0xD800 && unit <= 0xDBFF) {
            if (i + 1 >= bytes.size()) {
                out.clear();
                return make_error_code(Errc::DecodeFailure);
            }

            const char16_t low = static_cast<char16_t>((bytes[i] << 8) | bytes[i + 1]);
            i += 2;

            if (low < 0xDC00 || low > 0xDFFF) {
                out.clear();
                return make_error_code(Errc::DecodeFailure);
            }

            const char32_t codepoint = 0x10000 +
                ((static_cast<char32_t>(unit - 0xD800) << 10) | (low - 0xDC00));
            append_utf8(out, codepoint);
        } else {
            append_utf8(out, unit);
        }
    }
    return {};
}

/**
 * @brief Invert the digits of a string.
 * @param digits The digits to invert.
 * @return The inverted digits.
 */
std::string PduCodec::invert_digits(std::string_view digits)
{
    std::string out;
    out.reserve(digits.size() + 1);

    for (std::size_t i = 0; i + 1 < digits.size(); i += 2) {
        out.push_back(digits[i + 1]);
        out.push_back(digits[i]);
    }

    if (digits.size() & 1U) {
        out.push_back('F');
        out.push_back(digits.back());
    }

    return out;
}

/**
 * @brief Serialize the digits of a string.
 * @param inverted The inverted digits to serialize.
 * @return The serialized digits.
 */
std::string PduCodec::serialize_digits(std::string_view inverted)
{
    std::string out;
    out.reserve(inverted.size());

    for (std::size_t i = 0; i + 1 < inverted.size(); i += 2) {
        out.push_back(inverted[i + 1]);
        out.push_back(inverted[i]);
    }

    if (!out.empty() && out.back() == 'F') {
        out.pop_back();
    }

    return out;
}

/**
 * @brief Encode a message to a single-segment PDU hex string.
 * @param message The message to encode.
 * @param pdu_hex The PDU hex string.
 * @return The error code.
 */
std::error_code PduCodec::encode(const Message& message, std::string& pdu_hex)
{
    pdu_hex.clear();

    const std::string dest = normalize_address_digits(message.peer_address);
    if (dest.empty()) {
        return make_error_code(Errc::InvalidArgument);
    }

    switch (message.coding) {
    case DataCoding::Gsm7Bit: {
        std::string septets;
        if (const auto ec = utf8_to_gsm7(message.user_data, septets); ec) {
            return ec;
        }
        if (septets.size() > kGsm7SingleLimit) {
            return make_error_code(Errc::EncodeFailure);
        }
        return encode_one_segment(message, dest, septets, {}, nullptr, pdu_hex);
    }
    case DataCoding::Ucs2: {
        std::vector<std::uint8_t> payload;
        if (const auto ec = encode_ucs2(message.user_data, payload); ec) {
            return ec;
        }
        if (payload.size() > kOctetSingleLimit) {
            return make_error_code(Errc::EncodeFailure);
        }
        return encode_one_segment(message, dest, {}, payload, nullptr, pdu_hex);
    }
    case DataCoding::EightBit:
    default: {
        if (message.user_data.size() > kOctetSingleLimit) {
            return make_error_code(Errc::EncodeFailure);
        }
        std::vector<std::uint8_t> payload;
        payload.reserve(message.user_data.size());
        for (unsigned char ch : message.user_data) {
            payload.push_back(ch);
        }
        return encode_one_segment(message, dest, {}, payload, nullptr, pdu_hex);
    }
    }
}

/**
 * @brief Encode a message into one or more submit PDUs (concat UDH when needed).
 * @param message The message to encode.
 * @param pdu_hexes One hex PDU per segment.
 * @return The error code.
 */
std::error_code PduCodec::encode_segments(const Message& message, std::vector<std::string>& pdu_hexes)
{
    pdu_hexes.clear();

    const std::string dest = normalize_address_digits(message.peer_address);
    if (dest.empty()) {
        return make_error_code(Errc::InvalidArgument);
    }

    auto push_one = [&](std::string_view gsm7,
                        std::span<const std::uint8_t> octets,
                        const ConcatUdh* concat) -> std::error_code {
        std::string hex;
        if (const auto ec = encode_one_segment(message, dest, gsm7, octets, concat, hex); ec) {
            return ec;
        }
        pdu_hexes.push_back(std::move(hex));
        return {};
    };

    switch (message.coding) {
    case DataCoding::Gsm7Bit: {
        std::string septets;
        if (const auto ec = utf8_to_gsm7(message.user_data, septets); ec) {
            return ec;
        }
        if (septets.size() <= kGsm7SingleLimit) {
            return push_one(septets, {}, nullptr);
        }

        std::vector<std::string_view> chunks;
        for (std::size_t start = 0; start < septets.size();) {
            const std::size_t end = gsm7_chunk_end(septets, start, kGsm7ConcatPayload);
            if (end <= start) {
                return make_error_code(Errc::EncodeFailure);
            }
            chunks.emplace_back(septets.data() + start, end - start);
            start = end;
        }
        if (chunks.size() > kMaxConcatSegments) {
            return make_error_code(Errc::EncodeFailure);
        }

        ConcatUdh concat;
        concat.ref = make_concat_ref(message);
        concat.total = static_cast<std::uint8_t>(chunks.size());

        for (std::size_t i = 0; i < chunks.size(); ++i) {
            concat.seq = static_cast<std::uint8_t>(i + 1U);
            if (const auto ec = push_one(chunks[i], {}, &concat); ec) {
                return ec;
            }
        }
        return {};
    }
    case DataCoding::Ucs2: {
        std::vector<std::uint8_t> payload;
        if (const auto ec = encode_ucs2(message.user_data, payload); ec) {
            return ec;
        }
        if (payload.size() <= kOctetSingleLimit) {
            return push_one({}, payload, nullptr);
        }

        std::vector<std::vector<std::uint8_t>> chunks;
        if (const auto ec = split_ucs2_octets(payload, kOctetConcatPayload, chunks); ec) {
            return ec;
        }
        if (chunks.size() > kMaxConcatSegments) {
            return make_error_code(Errc::EncodeFailure);
        }

        ConcatUdh concat;
        concat.ref = make_concat_ref(message);
        concat.total = static_cast<std::uint8_t>(chunks.size());
        for (std::size_t i = 0; i < chunks.size(); ++i) {
            concat.seq = static_cast<std::uint8_t>(i + 1U);
            if (const auto ec = push_one({}, chunks[i], &concat); ec) {
                return ec;
            }
        }
        return {};
    }
    case DataCoding::EightBit:
    default: {
        if (message.user_data.size() <= kOctetSingleLimit) {
            std::vector<std::uint8_t> payload;
            payload.reserve(message.user_data.size());
            for (unsigned char ch : message.user_data) {
                payload.push_back(ch);
            }
            return push_one({}, payload, nullptr);
        }

        const std::size_t total =
            (message.user_data.size() + kOctetConcatPayload - 1U) / kOctetConcatPayload;
        if (total > kMaxConcatSegments) {
            return make_error_code(Errc::EncodeFailure);
        }

        ConcatUdh concat;
        concat.ref = make_concat_ref(message);
        concat.total = static_cast<std::uint8_t>(total);

        for (std::size_t i = 0; i < total; ++i) {
            concat.seq = static_cast<std::uint8_t>(i + 1U);
            const std::size_t off = i * kOctetConcatPayload;
            const std::size_t len = (std::min)(kOctetConcatPayload, message.user_data.size() - off);
            std::vector<std::uint8_t> chunk;
            chunk.reserve(len);
            for (std::size_t n = 0; n < len; ++n) {
                chunk.push_back(static_cast<std::uint8_t>(
                    static_cast<unsigned char>(message.user_data[off + n])));
            }
            if (const auto ec = push_one({}, chunk, &concat); ec) {
                return ec;
            }
        }
        return {};
    }
    }
}

/**
 * @brief Decode a PDU hex string to a message.
 * @param pdu_hex The PDU hex string.
 * @param message The message to decode.
 * @return The error code.
 */
std::error_code PduCodec::decode(std::string_view pdu_hex, Message& message)
{
    message = Message{};

    std::vector<std::uint8_t> bytes;

    if (const auto ec = hex_to_bytes(pdu_hex, bytes); ec) {
        return ec;
    }

    if (bytes.empty()) {
        return make_error_code(Errc::DecodeFailure);
    }

    std::size_t offset = 0;
    const std::uint8_t smsc_len = bytes[offset++];

    if (smsc_len > 0) {
        if (offset + smsc_len > bytes.size()) {
            return make_error_code(Errc::DecodeFailure);
        }

        const std::size_t digit_octets = static_cast<std::size_t>(smsc_len) - 1;
        ++offset; // TOA

        if (offset + digit_octets > bytes.size()) {
            return make_error_code(Errc::DecodeFailure);
        }

        const auto hex = bytes_to_hex(std::span<const std::uint8_t>{bytes.data() + offset, digit_octets});
        message.service_center = serialize_digits(hex);
        offset += digit_octets;
    }

    if (offset >= bytes.size()) {
        return make_error_code(Errc::DecodeFailure);
    }

    const std::uint8_t first_octet = bytes[offset++];
    const std::uint8_t mti = static_cast<std::uint8_t>(first_octet & 0x03U);
    const std::uint8_t vpf = static_cast<std::uint8_t>((first_octet >> 3) & 0x03U);
    const bool is_submit = (mti == 0x01U);
    // In SMS-SUBMIT this bit is TP-SRR.  In SMS-DELIVER it is TP-SRI, so do
    // not expose the latter as a request made by the sender.
    message.request_status_report = is_submit && ((first_octet & 0x20U) != 0U);

    if (mti == 0x02U) {
        // SMS-STATUS-REPORT: TP-MR, TP-RA, TP-SCTS, TP-DT, TP-Status,
        // followed by optional parameters described by TP-PI.
        message.is_status_report = true;

        if (offset >= bytes.size()) {
            return make_error_code(Errc::DecodeFailure);
        }

        message.message_reference = bytes[offset++];

        if (offset >= bytes.size()) {
            return make_error_code(Errc::DecodeFailure);
        }

        const std::uint8_t addr_len_digits = bytes[offset++];

        if (offset >= bytes.size()) {
            return make_error_code(Errc::DecodeFailure);
        }

        ++offset; // TP-RA TOA

        const std::size_t addr_octets =
            (static_cast<std::size_t>(addr_len_digits) + 1U) / 2U;

        if (offset + addr_octets > bytes.size()) {
            return make_error_code(Errc::DecodeFailure);
        }

        const auto address_hex =
            bytes_to_hex(std::span<const std::uint8_t>{bytes.data() + offset, addr_octets});

        message.peer_address = serialize_digits(address_hex);

        if (message.peer_address.size() > addr_len_digits) {
            message.peer_address.resize(addr_len_digits);
        }

        offset += addr_octets;

        if (offset + 15U > bytes.size()) {
            return make_error_code(Errc::DecodeFailure);
        }

        const auto service_hex =
            bytes_to_hex(std::span<const std::uint8_t>{bytes.data() + offset, 7});
        message.service_timestamp = serialize_digits(service_hex);
        offset += 7;

        const auto discharge_hex =
            bytes_to_hex(std::span<const std::uint8_t>{bytes.data() + offset, 7});

        message.discharge_time = serialize_digits(discharge_hex);
        offset += 7;
        message.tp_status = bytes[offset++];

        if (offset < bytes.size()) {
            const std::uint8_t parameter_indicator = bytes[offset++];

            if ((parameter_indicator & 0x01U) != 0U) {
                if (offset >= bytes.size()) {
                    return make_error_code(Errc::DecodeFailure);
                }

                message.protocol_id = bytes[offset++];
            }

            if ((parameter_indicator & 0x02U) != 0U) {
                if (offset >= bytes.size()) {
                    return make_error_code(Errc::DecodeFailure);
                }

                message.coding = static_cast<DataCoding>(bytes[offset++]);
            }

            if ((parameter_indicator & 0x04U) != 0U) {
                if (offset >= bytes.size()) {
                    return make_error_code(Errc::DecodeFailure);
                }

                const std::size_t udl = bytes[offset++];
                const std::size_t packed_len =
                    message.coding == DataCoding::Gsm7Bit
                        ? (udl * 7U + 7U) / 8U
                        : udl;
                if (offset + packed_len > bytes.size()) {
                    return make_error_code(Errc::DecodeFailure);
                }

                const auto payload =
                    std::span<const std::uint8_t>{bytes.data() + offset, packed_len};
                if (message.coding == DataCoding::Gsm7Bit) {
                    const auto septets = decode_7bit(payload, udl);

                    if (const auto ec = gsm7_to_utf8(septets, message.user_data); ec) {
                        return ec;
                    }
                } else if (message.coding == DataCoding::Ucs2) {
                    if (const auto ec = decode_ucs2(payload, message.user_data); ec) {
                        return ec;
                    }
                } else {
                    message.user_data.assign(
                        reinterpret_cast<const char*>(payload.data()), payload.size());
                }

                offset += packed_len;
            }
        }

        return {};
    }

    if (is_submit) {
        // SMS-SUBMIT carries TP-MR before the destination address.
        if (offset >= bytes.size()) {
            return make_error_code(Errc::DecodeFailure);
        }
        
        ++offset; // TP-MR
    }

    if (offset >= bytes.size()) {
        return make_error_code(Errc::DecodeFailure);
    }

    const std::uint8_t addr_len_digits = bytes[offset++];

    if (offset >= bytes.size()) {
        return make_error_code(Errc::DecodeFailure);
    }

    ++offset; // TOA

    const std::size_t addr_octets = (static_cast<std::size_t>(addr_len_digits) + 1U) / 2U;

    if (offset + addr_octets > bytes.size()) {
        return make_error_code(Errc::DecodeFailure);
    }
    {
        const auto hex = bytes_to_hex(std::span<const std::uint8_t>{bytes.data() + offset, addr_octets});
        message.peer_address = serialize_digits(hex);

        if (message.peer_address.size() > addr_len_digits) {
            message.peer_address.resize(addr_len_digits);
        }
    }
    offset += addr_octets;

    if (offset + 2 > bytes.size()) {
        return make_error_code(Errc::DecodeFailure);
    }

    message.protocol_id = bytes[offset++];
    message.coding = static_cast<DataCoding>(bytes[offset++]);

    if (is_submit) {
        // Skip TP-VP according to TP-VPF.
        std::size_t vp_len = 0;
        switch (vpf) {
        case 0x01: // enhanced
        case 0x03: // absolute
            vp_len = 7;
            break;
        case 0x02: // relative
            vp_len = 1;
            break;
        default:
            vp_len = 0;
            break;
        }

        if (offset + vp_len > bytes.size()) {
            return make_error_code(Errc::DecodeFailure);
        }
        
        if (vpf == 0x02) {
            message.relative_validity_period = bytes[offset];
        }
        offset += vp_len;
    } else {
        // SMS-DELIVER: TP-SCTS is always 7 octets.
        if (offset + 7 > bytes.size()) {
            return make_error_code(Errc::DecodeFailure);
        }

        const auto ts_hex = bytes_to_hex(std::span<const std::uint8_t>{bytes.data() + offset, 7});
        message.service_timestamp = serialize_digits(ts_hex);
        offset += 7;
    }

    if (offset >= bytes.size()) {
        return make_error_code(Errc::DecodeFailure);
    }

    const std::uint8_t udl = bytes[offset++];
    auto ud = std::span<const std::uint8_t>{bytes.data() + offset, bytes.size() - offset};

    const bool udhi = (first_octet & 0x40U) != 0U;
    message.has_udh = udhi;

    std::size_t udh_octets = 0;
    if (udhi) {
        if (ud.empty()) {
            return make_error_code(Errc::DecodeFailure);
        }

        udh_octets = static_cast<std::size_t>(ud[0]) + 1U; // UDHL + header body

        if (ud.size() < udh_octets) {
            return make_error_code(Errc::DecodeFailure);
        }

        parse_concat_from_udh(ud.first(udh_octets), message);
    }

    switch (message.coding) {
    case DataCoding::Gsm7Bit: {
        const std::size_t packed_len = (static_cast<std::size_t>(udl) * 7U + 7U) / 8U;

        if (ud.size() < packed_len) {
            return make_error_code(Errc::DecodeFailure);
        }

        // UDH occupies whole octets; fill bits align the first user septet.
        const std::size_t skip_septets =
            udhi ? ((udh_octets * 8U) + 6U) / 7U : 0U;
        if (skip_septets > udl) {
            return make_error_code(Errc::DecodeFailure);
        }

        auto septets = decode_7bit(ud.first(packed_len), udl);
        if (skip_septets > 0) {
            if (septets.size() < skip_septets) {
                return make_error_code(Errc::DecodeFailure);
            }
            septets.erase(0, skip_septets);
        }
        if (const auto ec = gsm7_to_utf8(septets, message.user_data); ec) {
            return ec;
        }

        break;
    }
    case DataCoding::Ucs2: {
        if (static_cast<std::size_t>(udl) < udh_octets || ud.size() < udl) {
            return make_error_code(Errc::DecodeFailure);
        }

        const auto payload = ud.subspan(udh_octets, static_cast<std::size_t>(udl) - udh_octets);

        if (const auto ec = decode_ucs2(payload, message.user_data); ec) {
            return ec;
        }

        break;
    }
    case DataCoding::EightBit:
    default: {
        if (static_cast<std::size_t>(udl) < udh_octets || ud.size() < udl) {
            return make_error_code(Errc::DecodeFailure);
        }

        const auto payload = ud.subspan(udh_octets, static_cast<std::size_t>(udl) - udh_octets);
        message.user_data.assign(reinterpret_cast<const char*>(payload.data()), payload.size());
        
        break;
    }
    }

    return {};
}

namespace {

/**
 * @brief Key for grouping concatenated SMS segments.
 * @param peer The peer address.
 * @param ref The reference number.
 * @param total Total number of segments in the group.
 * @param coding The coding scheme.
 */
struct ConcatGroupKey {
    std::string peer;
    std::uint16_t ref{0};
    std::uint8_t total{0};
    DataCoding coding{DataCoding::Ucs2};

    /**
     * @brief Compares two concatenated SMS group keys.
     * @param other The other key to compare to.
     * @return True if this key is less than the other key, false otherwise.
     */
    [[nodiscard]] bool operator<(const ConcatGroupKey& other) const
    {
        return std::tie(peer, ref, total, coding) <
               std::tie(other.peer, other.ref, other.total, other.coding);
    }
};

/**
 * @brief Checks if a message is a concatenated SMS segment.
 * @param message The message to check.
 * @return True if the message is a concatenated SMS segment, false otherwise.
 */
[[nodiscard]] bool is_concat_segment(const Message& message) noexcept
{
    return message.is_concatenated && message.concat_total > 0 && message.concat_seq > 0 &&
           message.concat_seq <= message.concat_total;
}

/**
 * @brief Merges a group of concatenated SMS segments into a single message.
 * @param by_seq Map of sequence numbers to messages.
 * @param total Total number of segments in the group.
 * @return Merged message.
 */
[[nodiscard]] Message merge_concat_group(const std::map<std::uint8_t, Message>& by_seq,
                                         std::uint8_t total)
{
    Message merged = by_seq.begin()->second;
    merged.has_udh = false;
    merged.is_concatenated = true;
    merged.concat_total = total;
    merged.concat_seq = 0;
    merged.user_data.clear();

    std::int16_t best_index = -1;

    for (std::uint8_t seq = 1; seq <= total; ++seq) {
        const Message& part = by_seq.at(seq);
        merged.user_data += part.user_data;

        if (part.index >= 0 && (best_index < 0 || part.index < best_index)) {
            best_index = part.index;
        }
    }

    merged.index = best_index;

    return merged;
}

} // namespace

/**
 * @brief Joins complete concatenated-SMS segment sets into single messages.
 * @param messages Decoded segments and/or single-part messages.
 * @return Reassembled list (incomplete groups kept as individual segments).
 */
std::vector<Message> PduCodec::reassemble_messages(std::vector<Message> messages)
{
    enum class SlotKind : std::uint8_t { Single, Group };

    struct OrderSlot {
        SlotKind kind{SlotKind::Single};
        std::size_t single_index{0};
        ConcatGroupKey key{};
    };

    std::map<ConcatGroupKey, std::map<std::uint8_t, Message>> groups;
    std::vector<OrderSlot> order;
    order.reserve(messages.size());

    for (std::size_t i = 0; i < messages.size(); ++i) {
        Message& message = messages[i];
        if (!is_concat_segment(message)) {
            order.push_back(OrderSlot{SlotKind::Single, i, {}});
            continue;
        }

        ConcatGroupKey key;
        key.peer = message.peer_address;
        key.ref = message.concat_ref;
        key.total = message.concat_total;
        key.coding = message.coding;

        auto& bucket = groups[key];

        if (bucket.find(message.concat_seq) != bucket.end()) {
            // Duplicate seq for this group — leave as a standalone entry.
            order.push_back(OrderSlot{SlotKind::Single, i, {}});
            continue;
        }

        if (bucket.empty()) {
            order.push_back(OrderSlot{SlotKind::Group, 0, key});
        }

        bucket.emplace(message.concat_seq, std::move(message));
    }

    std::vector<Message> out;
    out.reserve(messages.size());

    for (const OrderSlot& slot : order) {
        if (slot.kind == SlotKind::Single) {
            out.push_back(std::move(messages[slot.single_index]));
            continue;
        }

        auto it = groups.find(slot.key);

        if (it == groups.end()) {
            continue;
        }

        auto& by_seq = it->second;
        const std::uint8_t total = slot.key.total;
        bool complete = by_seq.size() == static_cast<std::size_t>(total);

        if (complete) {
            for (std::uint8_t seq = 1; seq <= total; ++seq) {
                if (by_seq.find(seq) == by_seq.end()) {
                    complete = false;
                    break;
                }
            }
        }

        if (complete) {
            out.push_back(merge_concat_group(by_seq, total));
        } else {
            for (auto& [seq, part] : by_seq) {
                (void)seq;
                out.push_back(std::move(part));
            }
        }

        groups.erase(it);
    }

    return out;
}

} // namespace comettextel
