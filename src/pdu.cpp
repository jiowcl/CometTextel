/**
 * @file pdu.cpp
 * @brief GSM 03.40 PDU encode/decode implementation.
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @email jiowcl@gmail.com
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#include "comettextel/pdu.hpp"

#include <cctype>
#include <cstring>

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
 * @brief Encode 7-bit text to bytes.
 * @param text The text to encode.
 * @return The encoded bytes.
 */
std::vector<std::uint8_t> PduCodec::encode_7bit(std::string_view text)
{
    std::vector<std::uint8_t> out;
    out.reserve((text.size() * 7 + 7) / 8);

    std::uint8_t left = 0;

    for (std::size_t n_src = 0; n_src < text.size(); ++n_src) {
        const int n_char = static_cast<int>(n_src & 7);
        const auto value = static_cast<std::uint8_t>(text[n_src] & 0x7F);

        if (n_char == 0) {
            left = value;
        } else {
            out.push_back(static_cast<std::uint8_t>((value << (8 - n_char)) | left));
            left = static_cast<std::uint8_t>(value >> n_char);
        }
    }

    // Incomplete septet groups still need the residual bits flushed.
    if (!text.empty() && (text.size() & 7U) != 0U) {
        out.push_back(left);
    }

    return out;
}

/**
 * @brief Decode 7-bit bytes to text.
 * @param packed The bytes to decode.
 * @param septet_count The number of septets to decode.
 * @return The decoded text.
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
 * @brief Encode a message to a PDU hex string.
 * @param message The message to encode.
 * @param pdu_hex The PDU hex string.
 * @return The error code.
 */
std::error_code PduCodec::encode(const Message& message, std::string& pdu_hex)
{
    pdu_hex.clear();

    const std::string smsc = normalize_address_digits(message.service_center);
    const std::string dest = normalize_address_digits(message.peer_address);

    if (dest.empty()) {
        return make_error_code(Errc::InvalidArgument);
    }

    std::vector<std::uint8_t> buf;
    buf.reserve(256);

    // SMSC address information.
    if (smsc.empty()) {
        buf.push_back(0x00); // length 0 => use modem default SMSC
    } else {
        const auto smsc_len = static_cast<std::uint8_t>(((smsc.size() & 1U) == 0 ? smsc.size() : smsc.size() + 1) / 2 + 1);
        buf.push_back(smsc_len);
        buf.push_back(0x91); // international
        const std::string inverted = invert_digits(smsc);
        std::vector<std::uint8_t> smsc_bytes;

        if (const auto ec = hex_to_bytes(inverted, smsc_bytes); ec) {
            return ec;
        }

        buf.insert(buf.end(), smsc_bytes.begin(), smsc_bytes.end());
    }

    // TPDU header + destination address.
    buf.push_back(0x11); // SMS-SUBMIT, relative VP
    buf.push_back(0x00); // TP-MR
    buf.push_back(static_cast<std::uint8_t>(dest.size()));
    buf.push_back(0x91); // international

    {
        const std::string inverted = invert_digits(dest);
        std::vector<std::uint8_t> dest_bytes;

        if (const auto ec = hex_to_bytes(inverted, dest_bytes); ec) {
            return ec;
        }

        buf.insert(buf.end(), dest_bytes.begin(), dest_bytes.end());
    }

    buf.push_back(message.protocol_id);
    buf.push_back(static_cast<std::uint8_t>(message.coding));
    buf.push_back(0x00); // TP-VP relative = 5 minutes

    std::vector<std::uint8_t> payload;

    switch (message.coding) {
    case DataCoding::Gsm7Bit: {
        // Single-segment limit without UDH: 160 septets (GSM 03.40).
        if (message.user_data.size() > 160) {
            return make_error_code(Errc::EncodeFailure);
        }

        const auto packed = encode_7bit(message.user_data);
        buf.push_back(static_cast<std::uint8_t>(message.user_data.size()));
        buf.insert(buf.end(), packed.begin(), packed.end());

        break;
    }
    case DataCoding::Ucs2: {
        if (const auto ec = encode_ucs2(message.user_data, payload); ec) {
            return ec;
        }

        if (payload.size() > 140) {
            return make_error_code(Errc::EncodeFailure);
        }

        buf.push_back(static_cast<std::uint8_t>(payload.size()));
        buf.insert(buf.end(), payload.begin(), payload.end());

        break;
    }
    case DataCoding::EightBit:
    default: {
        if (message.user_data.size() > 140) {
            return make_error_code(Errc::EncodeFailure);
        }

        buf.push_back(static_cast<std::uint8_t>(message.user_data.size()));

        for (unsigned char ch : message.user_data) {
            buf.push_back(ch);
        }

        break;
    }
    }

    pdu_hex = bytes_to_hex(buf);

    return {};
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

        auto decoded = decode_7bit(ud.first(packed_len), udl);
        if (skip_septets > 0) {
            if (decoded.size() < skip_septets) {
                return make_error_code(Errc::DecodeFailure);
            }
            decoded.erase(0, skip_septets);
        }
        message.user_data = std::move(decoded);

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

} // namespace comettextel
