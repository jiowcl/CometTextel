/**
 * @file pdu_tpdu_submit.cpp
 * @brief GSM 03.40 SMS-SUBMIT TPDU encode (internal).
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#include "comettextel/pdu.hpp"

#include "pdu/detail/gsm7.hpp"
#include "pdu/detail/tpdu_common.hpp"
#include "pdu/detail/tpdu_limits.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace comettextel {
namespace {

using detail::tpdu::ConcatUdh;
using detail::tpdu::kConcatUdhOctets;
using detail::tpdu::kGsm7ConcatPayload;
using detail::tpdu::kGsm7ConcatSkipSeptets;
using detail::tpdu::kGsm7SingleLimit;
using detail::tpdu::kMaxConcatSegments;
using detail::tpdu::kOctetConcatPayload;
using detail::tpdu::kOctetSingleLimit;

/**
 * @brief Makes a concat reference number.
 * @param message The message to make the reference number for.
 * @return The reference number.
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
    const std::string smsc = detail::tpdu::normalize_address_digits(message.service_center);

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
 * @brief Encodes a message into a PDU hex string.
 * @param message The message to encode.
 * @param pdu_hex The PDU hex string to encode to.
 * @return The error code.
 */
std::error_code PduCodec::encode(const Message& message, std::string& pdu_hex)
{
    pdu_hex.clear();

    const std::string dest = detail::tpdu::normalize_address_digits(message.peer_address);
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

    const std::string dest = detail::tpdu::normalize_address_digits(message.peer_address);
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
            const std::size_t end = detail::gsm7::chunk_end(septets, start, kGsm7ConcatPayload);
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

} // namespace comettextel
