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
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "comettextel/pdu.hpp"

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
    ++offset;                           // skip relative VP

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

} // namespace

int main()
{
    test_hex_roundtrip();
    test_digit_roundtrip();
    test_7bit_roundtrip();
    test_ucs2_roundtrip();
    test_pdu_submit_roundtrip_gsm7bit();
    test_pdu_submit_roundtrip_ucs2();
    test_pdu_submit_roundtrip_ucs2_cjk();
    test_pdu_submit_roundtrip_eightbit();
    test_pdu_submit_default_smsc();
    test_pdu_deliver_path();
    test_encode_rejects_empty_destination();

    if (g_failures != 0) {
        std::cerr << g_failures << " check(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All PDU round-trip tests passed\n";
    return EXIT_SUCCESS;
}
