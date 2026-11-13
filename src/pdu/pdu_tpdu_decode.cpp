/**
 * @file pdu_tpdu_decode.cpp
 * @brief GSM 03.40 TPDU decode and concat reassembly (internal).
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#include "comettextel/pdu.hpp"

#include "pdu/detail/tpdu_common.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace comettextel {

/**
 * @brief Decodes a PDU hex string into a message.
 * @param pdu_hex The PDU hex string to decode.
 * @param message The message to decode to.
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

        detail::tpdu::parse_concat_from_udh(ud.first(udh_octets), message);
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
