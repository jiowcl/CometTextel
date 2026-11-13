/**
 * @file pdu.cpp
 * @brief PDU hex / address / UCS-2 helpers (TPDU submit/decode in pdu_tpdu_*.cpp).
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @email jiowcl@gmail.com
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#include "comettextel/pdu.hpp"

#include "pdu/detail/utf8_text.hpp"

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
        if (!detail::utf8_next(utf8, index, codepoint)) {
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
            detail::append_utf8(out, codepoint);
        } else {
            detail::append_utf8(out, unit);
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

} // namespace comettextel
