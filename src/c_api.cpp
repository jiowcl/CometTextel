/**
 * @file c_api.cpp
 * @brief C ABI implementation wrapping the C++ CometTextel API.
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @email jiowcl@gmail.com
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#include "comettextel/c_api.h"

#include "comettextel/modem.hpp"
#include "comettextel/pdu.hpp"

#include <chrono>
#include <cstring>
#include <new>
#include <string>

struct ct_modem {
    comettextel::GsmModem impl;
};

namespace {

/**
 * @brief Map a CometTextel error code to a C API error code.
 * @param ec The CometTextel error code.
 * @return The C API error code.
 */
[[nodiscard]] int map_error(const std::error_code& ec)
{
    if (!ec) {
        return CT_OK;
    }
    if (ec.category() != comettextel::error_category()) {
        return CT_ERR_UNKNOWN;
    }

    switch (static_cast<comettextel::Errc>(ec.value())) {
    case comettextel::Errc::Ok:
        return CT_OK;
    case comettextel::Errc::InvalidArgument:
        return CT_ERR_INVALID_ARGUMENT;
    case comettextel::Errc::NotOpen:
        return CT_ERR_NOT_OPEN;
    case comettextel::Errc::AlreadyOpen:
        return CT_ERR_ALREADY_OPEN;
    case comettextel::Errc::IoFailure:
        return CT_ERR_IO;
    case comettextel::Errc::Timeout:
        return CT_ERR_TIMEOUT;
    case comettextel::Errc::ModemRejected:
        return CT_ERR_MODEM_REJECTED;
    case comettextel::Errc::EncodeFailure:
        return CT_ERR_ENCODE;
    case comettextel::Errc::DecodeFailure:
        return CT_ERR_DECODE;
    case comettextel::Errc::Unsupported:
        return CT_ERR_UNSUPPORTED;
    }
    return CT_ERR_UNKNOWN;
}

/**
 * @brief Map a C API data coding scheme to a CometTextel data coding scheme.
 * @param dcs The C API data coding scheme.
 * @return The CometTextel data coding scheme.
 */
[[nodiscard]] comettextel::DataCoding map_dcs(int dcs)
{
    switch (dcs) {
    case CT_DCS_GSM7:
        return comettextel::DataCoding::Gsm7Bit;
    case CT_DCS_8BIT:
        return comettextel::DataCoding::EightBit;
    case CT_DCS_UCS2:
    default:
        return comettextel::DataCoding::Ucs2;
    }
}

/**
 * @brief Copy a string to a buffer.
 * @param dest The destination buffer.
 * @param dest_cap The destination buffer capacity.
 * @param src The source string.
 */
void copy_field(char* dest, std::size_t dest_cap, std::string_view src)
{
    if (dest == nullptr || dest_cap == 0) {
        return;
    }

    const std::size_t n = src.size() < (dest_cap - 1) ? src.size() : (dest_cap - 1);

    if (n > 0) {
        std::memcpy(dest, src.data(), n);
    }

    dest[n] = '\0';
}

/**
 * @brief Fill a C API message struct with a CometTextel message.
 * @param out The destination message struct.
 * @param msg The CometTextel message.
 */
void fill_message(ct_message* out, const comettextel::Message& msg)
{
    if (out == nullptr) {
        return;
    }

    std::memset(out, 0, sizeof(*out));
    out->index = msg.index;
    out->dcs = static_cast<int32_t>(msg.coding);
    out->has_udh = msg.has_udh ? 1 : 0;

    copy_field(out->service_center, sizeof(out->service_center), msg.service_center);
    copy_field(out->peer_address, sizeof(out->peer_address), msg.peer_address);
    copy_field(out->service_timestamp, sizeof(out->service_timestamp), msg.service_timestamp);
    copy_field(out->user_data, sizeof(out->user_data), msg.user_data);
}

} // namespace

extern "C" {

/**
 * @brief Get the string representation of a C API status code.
 * @param status The C API status code.
 * @return The string representation of the status code.
 */
const char* ct_status_string(int status)
{
    switch (status) {
    case CT_OK:
        return "success";
    case CT_ERR_INVALID_ARGUMENT:
        return "invalid argument";
    case CT_ERR_NOT_OPEN:
        return "serial port is not open";
    case CT_ERR_ALREADY_OPEN:
        return "serial port is already open";
    case CT_ERR_IO:
        return "I/O failure";
    case CT_ERR_TIMEOUT:
        return "operation timed out";
    case CT_ERR_MODEM_REJECTED:
        return "modem rejected the command";
    case CT_ERR_ENCODE:
        return "PDU encode failure";
    case CT_ERR_DECODE:
        return "PDU decode failure";
    case CT_ERR_UNSUPPORTED:
        return "unsupported operation";
    default:
        return "unknown error";
    }
}

/**
 * @brief Create a new C API modem object.
 * @return The new modem object.
 */
ct_modem* ct_modem_create(void)
{
    try {
        return new ct_modem{};
    } catch (...) {
        return nullptr;
    }
}

/**
 * @brief Destroy a C API modem object.
 * @param modem The modem object to destroy.
 */
void ct_modem_destroy(ct_modem* modem)
{
    delete modem;
}

/**
 * @brief Open a serial port and initialize the modem.
 * @param modem The modem object.
 * @param port The serial port to open.
 * @param baud_rate The baud rate to use.
 * @return The C API status code.
 */
int ct_modem_open(ct_modem* modem, const char* port, uint32_t baud_rate)
{
    if (modem == nullptr || port == nullptr || port[0] == '\0') {
        return CT_ERR_INVALID_ARGUMENT;
    }

    comettextel::SerialConfig config;
    config.baud_rate = baud_rate == 0 ? 115200U : baud_rate;

    return map_error(modem->impl.open_and_init(port, config));
}

/**
 * @brief Send a message using the modem.
 * @param modem The modem object.
 * @param smsc The service center address.
 * @param destination The destination address.
 * @param text The message text.
 * @param dcs The data coding scheme.
 * @param timeout_ms The timeout in milliseconds.
 * @return The C API status code.
 */
int ct_modem_send(ct_modem* modem,
                  const char* smsc,
                  const char* destination,
                  const char* text,
                  int dcs,
                  int timeout_ms)
{
    if (modem == nullptr || destination == nullptr || text == nullptr) {
        return CT_ERR_INVALID_ARGUMENT;
    }

    comettextel::Message message;
    message.service_center = smsc != nullptr ? smsc : "";
    message.peer_address = destination;
    message.user_data = text;
    message.coding = map_dcs(dcs);

    const auto timeout = std::chrono::milliseconds(timeout_ms > 0 ? timeout_ms : 10000);

    return map_error(modem->impl.send_message(message, nullptr, timeout));
}

/**
 * @brief List messages from the modem.
 * @param modem The modem object.
 * @param out The destination message list.
 * @param max_count The maximum number of messages to list.
 * @param out_count The number of messages listed.
 * @param timeout_ms The timeout in milliseconds.
 * @return The C API status code.
 */
int ct_modem_list(ct_modem* modem,
                  ct_message* out,
                  int max_count,
                  int* out_count,
                  int timeout_ms)
{
    if (modem == nullptr || out == nullptr || max_count <= 0 || out_count == nullptr) {
        return CT_ERR_INVALID_ARGUMENT;
    }

    *out_count = 0;
    
    if (const auto ec = modem->impl.request_message_list(); ec) {
        return map_error(ec);
    }

    comettextel::ResponseBuffer buffer;
    const auto timeout = std::chrono::milliseconds(timeout_ms > 0 ? timeout_ms : 8000);

    if (const auto ec = modem->impl.wait_until_ok(buffer, timeout); ec) {
        return map_error(ec);
    }

    const auto messages = comettextel::GsmModem::parse_message_list(buffer);
    const int n = static_cast<int>(messages.size()) < max_count
                      ? static_cast<int>(messages.size())
                      : max_count;
    for (int i = 0; i < n; ++i) {
        fill_message(&out[i], messages[static_cast<std::size_t>(i)]);
    }

    *out_count = n;

    return CT_OK;
}


/**
 * @brief Delete a message from the modem.
 * @param modem The modem object.
 * @param index The index of the message to delete.
 * @param timeout_ms The timeout in milliseconds.
 * @return The C API status code.
 */
int ct_modem_delete(ct_modem* modem, int index, int timeout_ms)
{
    if (modem == nullptr) {
        return CT_ERR_INVALID_ARGUMENT;
    }

    if (const auto ec = modem->impl.delete_message(index); ec) {
        return map_error(ec);
    }

    comettextel::ResponseBuffer buffer;
    const auto timeout = std::chrono::milliseconds(timeout_ms > 0 ? timeout_ms : 5000);

    return map_error(modem->impl.wait_until_ok(buffer, timeout));
}

/**
 * @brief Encode a PDU submit message.
 * @param smsc The service center address.
 * @param destination The destination address.
 * @param text The message text.
 * @param dcs The data coding scheme.
 * @param out_hex The destination PDU hex string.
 * @param out_hex_cap The destination PDU hex string capacity.
 * @return The C API status code.
 */
int ct_pdu_encode_submit(const char* smsc,
                         const char* destination,
                         const char* text,
                         int dcs,
                         char* out_hex,
                         size_t out_hex_cap)
{
    if (destination == nullptr || text == nullptr || out_hex == nullptr || out_hex_cap == 0) {
        return CT_ERR_INVALID_ARGUMENT;
    }

    comettextel::Message message;
    message.service_center = smsc != nullptr ? smsc : "";
    message.peer_address = destination;
    message.user_data = text;
    message.coding = map_dcs(dcs);

    std::string hex;

    if (const auto ec = comettextel::PduCodec::encode(message, hex); ec) {
        return map_error(ec);
    }

    if (hex.size() + 1 > out_hex_cap) {
        return CT_ERR_ENCODE;
    }

    std::memcpy(out_hex, hex.c_str(), hex.size() + 1);

    return CT_OK;
}


/**
 * @brief Decode a PDU message.
 * @param pdu_hex The PDU hex string.
 * @param out The destination message struct.
 * @return The C API status code.
 */
int ct_pdu_decode(const char* pdu_hex, ct_message* out)
{
    if (pdu_hex == nullptr || out == nullptr) {
        return CT_ERR_INVALID_ARGUMENT;
    }

    comettextel::Message message;

    if (const auto ec = comettextel::PduCodec::decode(pdu_hex, message); ec) {
        return map_error(ec);
    }

    fill_message(out, message);
    
    return CT_OK;
}

} // extern "C"
