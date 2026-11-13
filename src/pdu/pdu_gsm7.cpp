/**
 * @file pdu_gsm7.cpp
 * @brief GSM 03.38 7-bit alphabet and packing (PduCodec GSM7 surface).
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#include "comettextel/pdu.hpp"

#include <algorithm>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "pdu/detail/gsm7.hpp"
#include "pdu/detail/utf8_text.hpp"

namespace comettextel {
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

/**
 * @brief GSM 03.38 default alphabet extension table (after ESC).
 * @param code The code to lookup.
 * @param cp The codepoint to lookup.
 */
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
 * @brief Encodes a UTF-8 codepoint to a GSM-7 septet string.
 * @param cp The codepoint to encode.
 * @param septets The string to encode to.
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
 * @brief Looks up a GSM-7 extension codepoint.
 * @param code The code to lookup.
 * @return The codepoint.
 */
[[nodiscard]] char32_t gsm7_ext_codepoint(std::uint8_t code) noexcept
{
    for (const GsmExtEntry& entry : kGsmExt) {
        if (entry.code == code) {
            return entry.cp;
        }
    }

    return kGsmBasic[code & 0x7F];
}

} // namespace

namespace detail::gsm7 {

/**
 * @brief End index for a GSM-7 septet chunk that does not split ESC pairs.
 * @param septets The string to check.
 * @param start The start index to check.
 * @param max_septets The maximum number of septets to check.
 * @return The end index.
 */
std::size_t chunk_end(std::string_view septets, std::size_t start, std::size_t max_septets) noexcept
{
    if (start >= septets.size() || max_septets == 0) {
        return start;
    }

    std::size_t end = start + (std::min)(max_septets, septets.size() - start);
    if (end > start && static_cast<unsigned char>(septets[end - 1]) == kGsmEsc && end < septets.size()) {
        --end;
    }

    if (end == start) {
        if (static_cast<unsigned char>(septets[start]) == kGsmEsc && start + 1 < septets.size()) {
            if (max_septets < 2) {
                return start;
            }
            return start + 2;
        }
        return start + 1;
    }

    return end;
}

} // namespace detail::gsm7

/**
 * @brief Encodes a GSM-7 septet string to a packed binary string.
 * @param septets The string to encode.
 * @return The packed binary string.
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

    if (!septets.empty() && (septets.size() & 7U) != 0U) {
        out.push_back(left);
    }

    return out;
}

/**
 * @brief Decodes a packed binary string to a GSM-7 septet string.
 * @param packed The packed binary string to decode.
 * @param septet_count The number of septets to decode.
 * @return The GSM-7 septet string.
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
 * @brief Converts a UTF-8 string to a GSM-7 septet string.
 * @param utf8 The UTF-8 string to convert.
 * @param septets The GSM-7 septet string to convert to.
 * @return The error code.
 */
std::error_code PduCodec::utf8_to_gsm7(std::string_view utf8, std::string& septets)
{
    septets.clear();
    septets.reserve(utf8.size());

    std::size_t index = 0;
    char32_t codepoint = 0;

    while (index < utf8.size()) {
        if (!detail::utf8_next(utf8, index, codepoint)) {
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
 * @brief Converts a GSM-7 septet string to a UTF-8 string.
 * @param septets The GSM-7 septet string to convert.
 * @param utf8 The UTF-8 string to convert to.
 * @return The error code.
 */
std::error_code PduCodec::gsm7_to_utf8(std::string_view septets, std::string& utf8)
{
    utf8.clear();
    utf8.reserve(septets.size());

    for (std::size_t i = 0; i < septets.size(); ++i) {
        const auto value = static_cast<std::uint8_t>(septets[i] & 0x7F);

        if (value == kGsmEsc) {
            if (i + 1 >= septets.size()) {
                detail::append_utf8(utf8, U' ');
                break;
            }

            const auto ext = static_cast<std::uint8_t>(septets[++i] & 0x7F);

            if (ext == kGsmEsc) {
                detail::append_utf8(utf8, U' ');
            } else {
                detail::append_utf8(utf8, gsm7_ext_codepoint(ext));
            }
            continue;
        }

        detail::append_utf8(utf8, kGsmBasic[value]);
    }

    return {};
}

} // namespace comettextel
