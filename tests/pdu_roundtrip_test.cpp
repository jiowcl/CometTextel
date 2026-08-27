/**
 * @file pdu_roundtrip_test.cpp
 * @brief Unit tests for PDU helper and full encode/decode round-trips.
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @email jiowcl@gmail.com
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "comettextel/modem.hpp"
#include "comettextel/pdu.hpp"
#if defined(COMETTEXTEL_HAS_C_API)
#include "comettextel/c_api.h"
#endif

namespace {

int g_failures = 0;

void expect(bool condition, std::string_view expression, std::string_view file, int line)
{
    if (!condition) {
        std::cerr << file << ':' << line << ": CHECK failed: " << expression << '\n';
        ++g_failures;
    }
}

#define CHECK(expr) ::expect(static_cast<bool>(expr), #expr, __FILE__, __LINE__)

void test_hex_roundtrip()
{
    const std::uint8_t raw[] = {0xC8, 0x32, 0x9B, 0xFD, 0x0E, 0x01};
    const auto hex = comettextel::PduCodec::bytes_to_hex(raw);
    CHECK(hex == "C8329BFD0E01");

    std::vector<std::uint8_t> bytes;
    CHECK(!comettextel::PduCodec::hex_to_bytes(hex, bytes));
    CHECK(bytes.size() == sizeof(raw));
    CHECK(std::equal(bytes.begin(), bytes.end(), std::begin(raw)));

    CHECK(!comettextel::PduCodec::hex_to_bytes("c8329bfd0e01", bytes));
    CHECK(bytes.size() == sizeof(raw));

    CHECK(static_cast<bool>(comettextel::PduCodec::hex_to_bytes("ABC", bytes)));
    CHECK(static_cast<bool>(comettextel::PduCodec::hex_to_bytes("GG", bytes)));
}

void test_digit_roundtrip()
{
    CHECK(comettextel::PduCodec::invert_digits("8613851872468") == "683158812764F8");
    CHECK(comettextel::PduCodec::serialize_digits("683158812764F8") == "8613851872468");

    const std::string even = "886912345678";
    CHECK(comettextel::PduCodec::serialize_digits(comettextel::PduCodec::invert_digits(even)) == even);
}

void test_7bit_roundtrip()
{
    const char* samples[] = {
        "A",
        "Hello",
        "Hello world",
        "12345678",
        "ABCDEFGHI",
    };

    for (const char* text : samples) {
        const std::string_view view{text};
        const auto packed = comettextel::PduCodec::encode_7bit(view);
        const auto expected_len = (view.size() * 7U + 7U) / 8U;
        CHECK(packed.size() == expected_len);

        const auto decoded = comettextel::PduCodec::decode_7bit(packed, view.size());
        CHECK(decoded == text);
    }
}

void test_ucs2_roundtrip()
{
    const char* samples[] = {
        "Hello",
        "你好",
        "CometTextel",
        "emoji:\xF0\x9F\x98\x80",
    };

    for (const char* text : samples) {
        std::vector<std::uint8_t> encoded;
        CHECK(!comettextel::PduCodec::encode_ucs2(text, encoded));
        CHECK(!encoded.empty());

        std::string decoded;
        CHECK(!comettextel::PduCodec::decode_ucs2(encoded, decoded));
        CHECK(decoded == text);
    }
}

void expect_message_roundtrip(const comettextel::Message& original)
{
    std::string pdu_hex;
    CHECK(!comettextel::PduCodec::encode(original, pdu_hex));
    CHECK(!pdu_hex.empty());
    CHECK(pdu_hex.size() % 2U == 0U);

    comettextel::Message decoded;
    CHECK(!comettextel::PduCodec::decode(pdu_hex, decoded));

    CHECK(decoded.service_center == original.service_center);
    CHECK(decoded.peer_address == original.peer_address);
    CHECK(decoded.protocol_id == original.protocol_id);
    CHECK(decoded.coding == original.coding);
    CHECK(decoded.user_data == original.user_data);
}

void test_gsm7_alphabet_utf8_roundtrip()
{
    const char* samples[] = {
        "Hello",
        "@£$¥èéùìòÇ",
        "ÄÖÑÜäöñüà",
        "[]{}\\~^|€",
        "ΔΦΓΛΩ",
        "Line1\nLine2",
    };

    for (const char* text : samples) {
        std::string septets;
        CHECK(!comettextel::PduCodec::utf8_to_gsm7(text, septets));
        CHECK(!septets.empty());

        std::string utf8;
        CHECK(!comettextel::PduCodec::gsm7_to_utf8(septets, utf8));
        CHECK(utf8 == text);
    }
}

void test_gsm7_pdu_escape_and_at_sign()
{
    comettextel::Message message;
    message.service_center = "886932000000";
    message.peer_address = "886912345678";
    message.coding = comettextel::DataCoding::Gsm7Bit;
    message.user_data = "Cost: 10€ [ok]";
    expect_message_roundtrip(message);
}

void test_submit_defaults_to_no_validity_period()
{
    comettextel::Message message;
    message.service_center = "886932000000";
    message.peer_address = "886912345678";
    message.coding = comettextel::DataCoding::Gsm7Bit;
    message.user_data = "Hi";

    std::string pdu_hex;
    CHECK(!comettextel::PduCodec::encode(message, pdu_hex));

    std::vector<std::uint8_t> bytes;
    CHECK(!comettextel::PduCodec::hex_to_bytes(pdu_hex, bytes));
    const std::size_t first_octet_offset = 1U + bytes[0];
    CHECK(bytes[first_octet_offset] == 0x01U); // SMS-SUBMIT, no UDH, no VP/SRR

    comettextel::Message decoded;
    CHECK(!comettextel::PduCodec::decode(pdu_hex, decoded));
    CHECK(!decoded.relative_validity_period.has_value());
    CHECK(!decoded.request_status_report);
}

void test_submit_relative_validity_and_status_report()
{
    comettextel::Message message;
    message.service_center = "886932000000";
    message.peer_address = "886912345678";
    message.coding = comettextel::DataCoding::Gsm7Bit;
    message.user_data = "Hi";
    message.relative_validity_period = 0x00; // Explicitly five minutes.
    message.request_status_report = true;

    std::string pdu_hex;
    CHECK(!comettextel::PduCodec::encode(message, pdu_hex));

    std::vector<std::uint8_t> bytes;
    CHECK(!comettextel::PduCodec::hex_to_bytes(pdu_hex, bytes));
    const std::size_t first_octet_offset = 1U + bytes[0];
    CHECK(bytes[first_octet_offset] == 0x31U); // relative VP + TP-SRR

    comettextel::Message decoded;
    CHECK(!comettextel::PduCodec::decode(pdu_hex, decoded));
    CHECK(decoded.relative_validity_period.has_value());
    CHECK(*decoded.relative_validity_period == 0x00U);
    CHECK(decoded.request_status_report);
}

void test_submit_concat_carries_options()
{
    comettextel::Message message;
    message.service_center = "886932000000";
    message.peer_address = "886912345678";
    message.coding = comettextel::DataCoding::Gsm7Bit;
    message.user_data = std::string(161, 'A');
    message.relative_validity_period = 0x8FU;
    message.request_status_report = true;

    std::vector<std::string> parts;
    CHECK(!comettextel::PduCodec::encode_segments(message, parts));
    CHECK(parts.size() == 2);

    for (const auto& part : parts) {
        comettextel::Message decoded;
        CHECK(!comettextel::PduCodec::decode(part, decoded));
        CHECK(decoded.relative_validity_period.has_value());
        CHECK(*decoded.relative_validity_period == 0x8FU);
        CHECK(decoded.request_status_report);
    }
}

void test_gsm7_rejects_unsupported_glyph()
{
    comettextel::Message message;
    message.service_center = "886932000000";
    message.peer_address = "886912345678";
    message.coding = comettextel::DataCoding::Gsm7Bit;
    message.user_data = "你好"; // CJK not in GSM 03.38 default alphabet

    std::string pdu_hex;
    const auto ec = comettextel::PduCodec::encode(message, pdu_hex);
    CHECK(ec == comettextel::make_error_code(comettextel::Errc::EncodeFailure));
    CHECK(pdu_hex.empty());
}

void test_gsm7_escape_counts_as_two_septets()
{
    // 159 plain 'A' + one euro (ESC+0x65) => 161 septets → single encode fails.
    std::string text(159, 'A');
    text += "\xE2\x82\xAC"; // € UTF-8

    std::string septets;
    CHECK(!comettextel::PduCodec::utf8_to_gsm7(text, septets));
    CHECK(septets.size() == 161);

    comettextel::Message message;
    message.service_center = "886932000000";
    message.peer_address = "886912345678";
    message.coding = comettextel::DataCoding::Gsm7Bit;
    message.user_data = text;

    std::string pdu_hex;
    CHECK(comettextel::PduCodec::encode(message, pdu_hex) ==
          comettextel::make_error_code(comettextel::Errc::EncodeFailure));

    std::vector<std::string> parts;
    CHECK(!comettextel::PduCodec::encode_segments(message, parts));
    CHECK(parts.size() == 2);

    std::string joined;
    for (const auto& part : parts) {
        comettextel::Message decoded;
        CHECK(!comettextel::PduCodec::decode(part, decoded));
        joined += decoded.user_data;
    }
    CHECK(joined == text);
}

void test_gsm7_concat_does_not_split_escape_pair()
{
    // Each '[' is ESC+0x3C (2 septets). 81 × '[' => 162 septets → concat;
    // chunking must not split an ESC from its extension code.
    std::string text(81, '[');

    comettextel::Message message;
    message.service_center = "886932000000";
    message.peer_address = "886912345678";
    message.coding = comettextel::DataCoding::Gsm7Bit;
    message.user_data = text;
    message.concat_ref = 0x42;

    std::string septets;
    CHECK(!comettextel::PduCodec::utf8_to_gsm7(text, septets));
    CHECK(septets.size() == 162);

    std::vector<std::string> parts;
    CHECK(!comettextel::PduCodec::encode_segments(message, parts));
    CHECK(parts.size() >= 2);

    std::string joined;
    for (const auto& part : parts) {
        comettextel::Message decoded;
        CHECK(!comettextel::PduCodec::decode(part, decoded));
        CHECK(decoded.is_concatenated);
        joined += decoded.user_data;
    }
    CHECK(joined == text);
}

void test_pdu_submit_roundtrip_gsm7bit()
{
    comettextel::Message message;
    message.service_center = "886932000000";
    message.peer_address = "886912345678";
    message.protocol_id = 0;
    message.coding = comettextel::DataCoding::Gsm7Bit;
    message.user_data = "Hello";
    expect_message_roundtrip(message);
}

void test_pdu_submit_roundtrip_ucs2()
{
    comettextel::Message message;
    message.service_center = "886932000000";
    message.peer_address = "886987654321";
    message.protocol_id = 0;
    message.coding = comettextel::DataCoding::Ucs2;
    message.user_data = "Hello UCS2";
    expect_message_roundtrip(message);
}

void test_pdu_submit_roundtrip_ucs2_cjk()
{
    comettextel::Message message;
    message.service_center = "886932000000";
    message.peer_address = "886987654321";
    message.coding = comettextel::DataCoding::Ucs2;
    message.user_data = "你好";
    expect_message_roundtrip(message);
}

void test_pdu_submit_roundtrip_eightbit()
{
    comettextel::Message message;
    message.service_center = "886932000000";
    message.peer_address = "886911122233";
    message.protocol_id = 0;
    message.coding = comettextel::DataCoding::EightBit;
    message.user_data = std::string("\x01\x02\x03\xFF", 4) + "binary";
    expect_message_roundtrip(message);
}

void test_pdu_submit_default_smsc()
{
    comettextel::Message message;
    message.service_center.clear();
    message.peer_address = "886912345678";
    message.coding = comettextel::DataCoding::Ucs2;
    message.user_data = "No SMSC";
    expect_message_roundtrip(message);
}

void test_pdu_deliver_path()
{
    // Encode an SMS-SUBMIT, then reshape it into SMS-DELIVER layout and decode.
    comettextel::Message submit;
    submit.service_center = "886932000000";
    submit.peer_address = "886912345678";
    submit.coding = comettextel::DataCoding::Gsm7Bit;
    submit.user_data = "Hi";

    std::string submit_hex;
    CHECK(!comettextel::PduCodec::encode(submit, submit_hex));

    std::vector<std::uint8_t> bytes;
    CHECK(!comettextel::PduCodec::hex_to_bytes(submit_hex, bytes));

    std::size_t offset = 0;
    const std::uint8_t smsc_len = bytes[offset++];
    offset += smsc_len;

    std::vector<std::uint8_t> deliver;
    deliver.insert(deliver.end(), bytes.begin(),
                   bytes.begin() + static_cast<std::ptrdiff_t>(1U + smsc_len));
    deliver.push_back(0x04); // SMS-DELIVER
    offset += 2;             // skip SUBMIT first octet + TP-MR

    const std::uint8_t addr_digits = bytes[offset];
    const std::size_t addr_octets = (static_cast<std::size_t>(addr_digits) + 1U) / 2U;
    const std::size_t addr_block = 2U + addr_octets;
    deliver.insert(deliver.end(), bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                   bytes.begin() + static_cast<std::ptrdiff_t>(offset + addr_block));
    offset += addr_block;

    deliver.push_back(bytes[offset++]); // PID
    deliver.push_back(bytes[offset++]); // DCS

    // Dummy TP-SCTS (7 octets) => 14 digit timestamp after serialize.
    constexpr std::uint8_t kScts[7] = {0x21, 0x50, 0x70, 0x41, 0x80, 0x45, 0x23};
    deliver.insert(deliver.end(), std::begin(kScts), std::end(kScts));
    deliver.insert(deliver.end(), bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.end());

    const auto deliver_hex = comettextel::PduCodec::bytes_to_hex(deliver);
    comettextel::Message decoded;
    CHECK(!comettextel::PduCodec::decode(deliver_hex, decoded));
    CHECK(decoded.service_center == submit.service_center);
    CHECK(decoded.peer_address == submit.peer_address);
    CHECK(decoded.coding == submit.coding);
    CHECK(decoded.user_data == submit.user_data);
    CHECK(!decoded.service_timestamp.empty());
}

void test_status_report_fixtures()
{
    // SMS-STATUS-REPORT with no optional parameters:
    // MR=0x2A, RA=886912345678, SCTS=DT=21 50 70 41 80 45 23.
    constexpr char kDelivered[] =
        "00022A0C91889621436587215070418045232150704180452300";
    constexpr char kExpired[] =
        "00022A0C91889621436587215070418045232150704180452340";
    constexpr char kWithOptionalParameters[] =
        "00022A0C9188962143658721507041804523215070418045230007000002CF25";

    comettextel::Message delivered;
    CHECK(!comettextel::PduCodec::decode(kDelivered, delivered));
    CHECK(delivered.is_status_report);
    CHECK(delivered.message_reference == 0x2A);
    CHECK(delivered.peer_address == "886912345678");
    CHECK(delivered.service_timestamp == "12050714085432");
    CHECK(delivered.discharge_time == "12050714085432");
    CHECK(delivered.tp_status == 0x00);
    CHECK(delivered.user_data.empty());

    comettextel::Message expired;
    CHECK(!comettextel::PduCodec::decode(kExpired, expired));
    CHECK(expired.is_status_report);
    CHECK(expired.message_reference == 0x2A);
    CHECK(expired.tp_status == 0x40);

    comettextel::Message with_optional;
    CHECK(!comettextel::PduCodec::decode(kWithOptionalParameters, with_optional));
    CHECK(with_optional.is_status_report);
    CHECK(with_optional.protocol_id == 0x00);
    CHECK(with_optional.coding == comettextel::DataCoding::Gsm7Bit);
    CHECK(with_optional.user_data == "OK");
}

void test_status_report_rejects_truncated_fixture()
{
    constexpr char kTruncated[] =
        "00022A0C918896214365872150704180452321507041804523";
    comettextel::Message message;
    CHECK(comettextel::PduCodec::decode(kTruncated, message) ==
          comettextel::make_error_code(comettextel::Errc::DecodeFailure));
}

void test_encode_rejects_empty_destination()
{
    comettextel::Message message;
    message.peer_address.clear();
    message.user_data = "x";
    message.coding = comettextel::DataCoding::Gsm7Bit;

    std::string pdu_hex;
    const auto ec = comettextel::PduCodec::encode(message, pdu_hex);
    CHECK(ec == comettextel::make_error_code(comettextel::Errc::InvalidArgument));
    CHECK(pdu_hex.empty());
}

void test_encode_rejects_overlong_payload()
{
    comettextel::Message base;
    base.service_center = "886932000000";
    base.peer_address = "886912345678";

    {
        comettextel::Message message = base;
        message.coding = comettextel::DataCoding::Gsm7Bit;
        message.user_data.assign(161, 'A');
        std::string pdu_hex;
        const auto ec = comettextel::PduCodec::encode(message, pdu_hex);
        CHECK(ec == comettextel::make_error_code(comettextel::Errc::EncodeFailure));
        CHECK(pdu_hex.empty());
    }

    {
        comettextel::Message message = base;
        message.coding = comettextel::DataCoding::Gsm7Bit;
        message.user_data.assign(160, 'A');
        std::string pdu_hex;
        CHECK(!comettextel::PduCodec::encode(message, pdu_hex));
        CHECK(!pdu_hex.empty());
    }

    {
        comettextel::Message message = base;
        message.coding = comettextel::DataCoding::EightBit;
        message.user_data.assign(141, '\x01');
        std::string pdu_hex;
        const auto ec = comettextel::PduCodec::encode(message, pdu_hex);
        CHECK(ec == comettextel::make_error_code(comettextel::Errc::EncodeFailure));
    }

    {
        comettextel::Message message = base;
        message.coding = comettextel::DataCoding::Ucs2;
        message.user_data.assign(71, 'B');
        std::string pdu_hex;
        const auto ec = comettextel::PduCodec::encode(message, pdu_hex);
        CHECK(ec == comettextel::make_error_code(comettextel::Errc::EncodeFailure));
    }
}

void test_classify_response()
{
    using comettextel::GsmModem;
    using comettextel::ModemResponse;

    CHECK(GsmModem::classify_response("") == ModemResponse::Wait);
    CHECK(GsmModem::classify_response("\r\nOK\r\n") == ModemResponse::Ok);
    CHECK(GsmModem::classify_response("+CMGL: 1\r\n00...\r\n\r\nOK\r\n") == ModemResponse::Ok);
    CHECK(GsmModem::classify_response("OK\r\n") == ModemResponse::Ok);
    CHECK(GsmModem::classify_response("\r\nERROR\r\n") == ModemResponse::Error);
    CHECK(GsmModem::classify_response("+CMS ERROR: 500\r\n") == ModemResponse::Error);
    CHECK(GsmModem::classify_response("+CME ERROR: 3\r\n") == ModemResponse::Error);
    CHECK(GsmModem::classify_response("partial") == ModemResponse::Wait);
}

void test_decode_skips_udh_ucs2()
{
    // SMS-DELIVER, UDHI set, UCS-2 payload "Hi" after a 6-octet concat UDH.
    // UDH: UDHL=05, IEI=00, IEDL=03, ref=AA, total=02, seq=01
    constexpr std::string_view kPdu =
        "0044049121430008000000000000000A"
        "050003AA0201"
        "00480069";

    comettextel::Message message;
    CHECK(!comettextel::PduCodec::decode(kPdu, message));
    CHECK(message.has_udh);
    CHECK(message.is_concatenated);
    CHECK(message.concat_ref == 0xAA);
    CHECK(message.concat_total == 2);
    CHECK(message.concat_seq == 1);
    CHECK(message.coding == comettextel::DataCoding::Ucs2);
    CHECK(message.user_data == "Hi");
}

void test_decode_skips_udh_eightbit()
{
    constexpr std::string_view kPdu =
        "00440491214300040000000000000008"
        "050003AA0201"
        "4869";

    comettextel::Message message;
    CHECK(!comettextel::PduCodec::decode(kPdu, message));
    CHECK(message.has_udh);
    CHECK(message.is_concatenated);
    CHECK(message.concat_ref == 0xAA);
    CHECK(message.concat_total == 2);
    CHECK(message.concat_seq == 1);
    CHECK(message.coding == comettextel::DataCoding::EightBit);
    CHECK(message.user_data == "Hi");
}

void test_decode_concat_16bit_ref()
{
    // Same frame as UCS-2 UDH test, but IEI 0x08 / IEDL 0x04 / ref=0x1234 / total=3 / seq=2
    // UDHL=06, then 08 04 12 34 03 02, payload UCS-2 "Hi"
    constexpr std::string_view kPdu =
        "0044049121430008000000000000000B"
        "06080412340302"
        "00480069";

    comettextel::Message message;
    CHECK(!comettextel::PduCodec::decode(kPdu, message));
    CHECK(message.has_udh);
    CHECK(message.is_concatenated);
    CHECK(message.concat_ref == 0x1234);
    CHECK(message.concat_total == 3);
    CHECK(message.concat_seq == 2);
    CHECK(message.user_data == "Hi");
}

void test_decode_udh_without_concat_ie()
{
    // UDHL=03, IEI=FF (unknown), IEDL=01, data=00 — has_udh but not concatenated
    constexpr std::string_view kPdu =
        "00440491214300080000000000000008"
        "03FF0100"
        "00480069";

    comettextel::Message message;
    CHECK(!comettextel::PduCodec::decode(kPdu, message));
    CHECK(message.has_udh);
    CHECK(!message.is_concatenated);
    CHECK(message.concat_ref == 0);
    CHECK(message.concat_total == 0);
    CHECK(message.concat_seq == 0);
    CHECK(message.user_data == "Hi");
}

void test_encode_segments_matches_encode_when_short()
{
    comettextel::Message message;
    message.service_center = "886932000000";
    message.peer_address = "886912345678";
    message.coding = comettextel::DataCoding::Ucs2;
    message.user_data = "Hello";

    std::string single;
    CHECK(!comettextel::PduCodec::encode(message, single));

    std::vector<std::string> parts;
    CHECK(!comettextel::PduCodec::encode_segments(message, parts));
    CHECK(parts.size() == 1);
    CHECK(parts[0] == single);
}

void test_encode_segments_gsm7_concat()
{
    comettextel::Message message;
    message.service_center = "886932000000";
    message.peer_address = "886912345678";
    message.coding = comettextel::DataCoding::Gsm7Bit;
    message.user_data.assign(161, 'A');
    message.concat_ref = 0xAA;

    std::vector<std::string> parts;
    CHECK(!comettextel::PduCodec::encode_segments(message, parts));
    CHECK(parts.size() == 2);

    std::string joined;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        comettextel::Message decoded;
        CHECK(!comettextel::PduCodec::decode(parts[i], decoded));
        CHECK(decoded.has_udh);
        CHECK(decoded.is_concatenated);
        CHECK(decoded.concat_ref == 0xAA);
        CHECK(decoded.concat_total == 2);
        CHECK(decoded.concat_seq == static_cast<std::uint8_t>(i + 1));
        CHECK(decoded.peer_address == message.peer_address);
        joined += decoded.user_data;
    }
    CHECK(joined == message.user_data);
}

void test_encode_segments_ucs2_concat()
{
    comettextel::Message message;
    message.service_center = "886932000000";
    message.peer_address = "886912345678";
    message.coding = comettextel::DataCoding::Ucs2;
    message.user_data.assign(71, 'B');
    message.concat_ref = 0x12;

    std::vector<std::string> parts;
    CHECK(!comettextel::PduCodec::encode_segments(message, parts));
    CHECK(parts.size() == 2);

    std::string joined;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        comettextel::Message decoded;
        CHECK(!comettextel::PduCodec::decode(parts[i], decoded));
        CHECK(decoded.is_concatenated);
        CHECK(decoded.concat_ref == 0x12);
        CHECK(decoded.concat_total == 2);
        CHECK(decoded.concat_seq == static_cast<std::uint8_t>(i + 1));
        joined += decoded.user_data;
    }
    CHECK(joined == message.user_data);
}

void test_encode_segments_eightbit_concat()
{
    comettextel::Message message;
    message.service_center = "886932000000";
    message.peer_address = "886912345678";
    message.coding = comettextel::DataCoding::EightBit;
    message.user_data.assign(141, '\x01');
    message.concat_ref = 0x03;

    std::vector<std::string> parts;
    CHECK(!comettextel::PduCodec::encode_segments(message, parts));
    CHECK(parts.size() == 2);

    std::string joined;
    for (const auto& hex : parts) {
        comettextel::Message decoded;
        CHECK(!comettextel::PduCodec::decode(hex, decoded));
        CHECK(decoded.is_concatenated);
        joined += decoded.user_data;
    }
    CHECK(joined == message.user_data);
}

void test_reassemble_ucs2_complete()
{
    comettextel::Message message;
    message.service_center = "886932000000";
    message.peer_address = "886912345678";
    message.coding = comettextel::DataCoding::Ucs2;
    message.user_data.assign(71, 'B');
    message.concat_ref = 0x12;

    std::vector<std::string> parts;
    CHECK(!comettextel::PduCodec::encode_segments(message, parts));
    CHECK(parts.size() == 2);

    std::vector<comettextel::Message> decoded;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        comettextel::Message part;
        CHECK(!comettextel::PduCodec::decode(parts[i], part));
        part.index = static_cast<std::int16_t>(10 + static_cast<int>(i));
        decoded.push_back(std::move(part));
    }

    const auto joined = comettextel::PduCodec::reassemble_messages(std::move(decoded));
    CHECK(joined.size() == 1);
    CHECK(joined[0].is_reassembled_concat());
    CHECK(joined[0].is_concatenated);
    CHECK(joined[0].concat_ref == 0x12);
    CHECK(joined[0].concat_total == 2);
    CHECK(joined[0].concat_seq == 0);
    CHECK(!joined[0].has_udh);
    CHECK(joined[0].index == 10);
    CHECK(joined[0].user_data == message.user_data);
    CHECK(joined[0].peer_address == message.peer_address);
}

void test_reassemble_incomplete_keeps_segments()
{
    comettextel::Message message;
    message.service_center = "886932000000";
    message.peer_address = "886912345678";
    message.coding = comettextel::DataCoding::Ucs2;
    message.user_data.assign(71, 'C');
    message.concat_ref = 0x34;

    std::vector<std::string> parts;
    CHECK(!comettextel::PduCodec::encode_segments(message, parts));
    CHECK(parts.size() == 2);

    comettextel::Message only_first;
    CHECK(!comettextel::PduCodec::decode(parts[0], only_first));

    auto out = comettextel::PduCodec::reassemble_messages({only_first});
    CHECK(out.size() == 1);
    CHECK(out[0].is_concatenated);
    CHECK(!out[0].is_reassembled_concat());
    CHECK(out[0].concat_seq == 1);
    CHECK(out[0].user_data == only_first.user_data);
}

void test_reassemble_preserves_singles_and_order()
{
    comettextel::Message single;
    single.peer_address = "886911111111";
    single.coding = comettextel::DataCoding::Ucs2;
    single.user_data = "Solo";

    comettextel::Message long_msg;
    long_msg.service_center = "886932000000";
    long_msg.peer_address = "886912345678";
    long_msg.coding = comettextel::DataCoding::Ucs2;
    long_msg.user_data.assign(71, 'D');
    long_msg.concat_ref = 0x55;

    std::vector<std::string> parts;
    CHECK(!comettextel::PduCodec::encode_segments(long_msg, parts));
    CHECK(parts.size() == 2);

    // Arrive out of order: single, part2, part1
    comettextel::Message p2;
    comettextel::Message p1;
    CHECK(!comettextel::PduCodec::decode(parts[1], p2));
    CHECK(!comettextel::PduCodec::decode(parts[0], p1));

    auto out = comettextel::PduCodec::reassemble_messages({single, p2, p1});
    CHECK(out.size() == 2);
    CHECK(out[0].user_data == "Solo");
    CHECK(!out[0].is_concatenated);
    CHECK(out[1].is_reassembled_concat());
    CHECK(out[1].user_data == long_msg.user_data);
}

void test_parse_message_list_reassembles()
{
    comettextel::Message message;
    message.service_center = "886932000000";
    message.peer_address = "886912345678";
    message.coding = comettextel::DataCoding::Ucs2;
    message.user_data.assign(71, 'E');
    message.concat_ref = 0x77;

    std::vector<std::string> parts;
    CHECK(!comettextel::PduCodec::encode_segments(message, parts));
    CHECK(parts.size() == 2);

    comettextel::ResponseBuffer buffer;
    buffer.data = "+CMGL: 3,1,,,\r\n" + parts[0] + "\r\n" +
                  "+CMGL: 4,1,,,\r\n" + parts[1] + "\r\n" +
                  "OK\r\n";

    const auto listed = comettextel::GsmModem::parse_message_list(buffer);
    CHECK(listed.size() == 1);
    CHECK(listed[0].is_reassembled_concat());
    CHECK(listed[0].index == 3);
    CHECK(listed[0].user_data == message.user_data);
}

void test_c_api_pdu_roundtrip()
{
#if defined(COMETTEXTEL_HAS_C_API)
    char hex[1024]{};
    CHECK(ct_pdu_encode_submit("886932000000", "886912345678", "Hello", CT_DCS_UCS2, hex,
                               sizeof(hex)) == CT_OK);
    CHECK(std::strlen(hex) > 0U);

    ct_message message{};
    CHECK(ct_pdu_decode(hex, &message) == CT_OK);
    CHECK(std::string_view{message.peer_address} == "886912345678");
    CHECK(std::string_view{message.user_data} == "Hello");
    CHECK(message.dcs == CT_DCS_UCS2);
    CHECK(message.is_concatenated == 0);
#else
    CHECK(true);
#endif
}

void test_c_api_submit_options()
{
#if defined(COMETTEXTEL_HAS_C_API)
    char hex[1024]{};
    CHECK(ct_pdu_encode_submit_ex("886932000000", "886912345678", "Hi", CT_DCS_GSM7,
                                  0x00, 1, hex, sizeof(hex)) == CT_OK);

    std::vector<std::uint8_t> bytes;
    CHECK(!comettextel::PduCodec::hex_to_bytes(hex, bytes));
    const std::size_t first_octet_offset = 1U + bytes[0];
    CHECK(bytes[first_octet_offset] == 0x31U);
#else
    CHECK(true);
#endif
}

void test_c_api_status_report()
{
#if defined(COMETTEXTEL_HAS_C_API)
    constexpr char kDelivered[] =
        "00022A0C91889621436587215070418045232150704180452300";
    ct_status_report report{};
    CHECK(ct_pdu_decode_status_report(kDelivered, &report) == CT_OK);
    CHECK(report.message_reference == 0x2A);
    CHECK(report.tp_status == 0x00);
    CHECK(std::string_view{report.recipient_address} == "886912345678");
    CHECK(std::string_view{report.service_timestamp} == "12050714085432");
    CHECK(std::string_view{report.discharge_time} == "12050714085432");
#else
    CHECK(true);
#endif
}

void test_c_api_concat_fields()
{
#if defined(COMETTEXTEL_HAS_C_API)
    constexpr char kPdu[] =
        "0044049121430008000000000000000A"
        "050003AA0201"
        "00480069";

    ct_message message{};
    CHECK(ct_pdu_decode(kPdu, &message) == CT_OK);
    CHECK(message.has_udh == 1);
    CHECK(message.is_concatenated == 1);
    CHECK(message.concat_ref == 0xAA);
    CHECK(message.concat_total == 2);
    CHECK(message.concat_seq == 1);
    CHECK(std::string_view{message.user_data} == "Hi");
#else
    CHECK(true);
#endif
}

void test_c_api_encode_segments()
{
#if defined(COMETTEXTEL_HAS_C_API)
    std::string text(71, 'B');
    char hex[8192]{};
    int count = 0;
    CHECK(ct_pdu_encode_submit_segments("886932000000", "886912345678", text.c_str(), CT_DCS_UCS2,
                                        hex, sizeof(hex), &count) == CT_OK);
    CHECK(count == 2);

    std::string joined;
    const char* cursor = hex;
    int seen = 0;
    while (*cursor != '\0') {
        const char* nl = std::strchr(cursor, '\n');
        std::string part = nl ? std::string(cursor, nl) : std::string(cursor);
        if (nl) {
            cursor = nl + 1;
        } else {
            cursor += part.size();
        }

        ct_message message{};
        CHECK(ct_pdu_decode(part.c_str(), &message) == CT_OK);
        CHECK(message.is_concatenated == 1);
        CHECK(message.concat_total == 2);
        CHECK(message.concat_seq == seen + 1);
        joined += message.user_data;
        ++seen;
    }
    CHECK(seen == 2);
    CHECK(joined == text);
#else
    CHECK(true);
#endif
}

} // namespace

int main()
{
    test_hex_roundtrip();
    test_digit_roundtrip();
    test_7bit_roundtrip();
    test_ucs2_roundtrip();
    test_gsm7_alphabet_utf8_roundtrip();
    test_gsm7_pdu_escape_and_at_sign();
    test_submit_defaults_to_no_validity_period();
    test_submit_relative_validity_and_status_report();
    test_submit_concat_carries_options();
    test_gsm7_rejects_unsupported_glyph();
    test_gsm7_escape_counts_as_two_septets();
    test_gsm7_concat_does_not_split_escape_pair();
    test_pdu_submit_roundtrip_gsm7bit();
    test_pdu_submit_roundtrip_ucs2();
    test_pdu_submit_roundtrip_ucs2_cjk();
    test_pdu_submit_roundtrip_eightbit();
    test_pdu_submit_default_smsc();
    test_pdu_deliver_path();
    test_status_report_fixtures();
    test_status_report_rejects_truncated_fixture();
    test_encode_rejects_empty_destination();
    test_encode_rejects_overlong_payload();
    test_classify_response();
    test_decode_skips_udh_ucs2();
    test_decode_skips_udh_eightbit();
    test_decode_concat_16bit_ref();
    test_decode_udh_without_concat_ie();
    test_encode_segments_matches_encode_when_short();
    test_encode_segments_gsm7_concat();
    test_encode_segments_ucs2_concat();
    test_encode_segments_eightbit_concat();
    test_reassemble_ucs2_complete();
    test_reassemble_incomplete_keeps_segments();
    test_reassemble_preserves_singles_and_order();
    test_parse_message_list_reassembles();
    test_c_api_pdu_roundtrip();
    test_c_api_submit_options();
    test_c_api_status_report();
    test_c_api_concat_fields();
    test_c_api_encode_segments();

    if (g_failures != 0) {
        std::cerr << g_failures << " check(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All PDU round-trip tests passed\n";
    return EXIT_SUCCESS;
}
