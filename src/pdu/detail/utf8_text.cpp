/**
 * @file utf8_text.cpp
 * @brief UTF-8 scalar walk / append (internal).
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#include "pdu/detail/utf8_text.hpp"

namespace comettextel::detail {

/**
 * @brief Advances the index to the next UTF-8 codepoint in a string.
 * @param text The string to advance in.
 * @param index The index to advance.
 * @param codepoint The codepoint to advance to.
 * @return True if the index was advanced, false otherwise.
 */
bool utf8_next(std::string_view text, std::size_t& index, char32_t& codepoint)
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
 * @brief Appends a UTF-8 codepoint to a string.
 * @param out The string to append to.
 * @param codepoint The codepoint to append.
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

} // namespace comettextel::detail
