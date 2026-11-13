/**
 * @file modem_messages.cpp
 * @brief SMS send (AT+CMGS), list (AT+CMGL), delete (AT+CMGD), and +CMGL parsing.
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#include "comettextel/modem.hpp"
#include "comettextel/pdu.hpp"

#include "modem/detail/cmgl.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace comettextel::detail::modem::cmgl {

/**
 * @brief Parses a message list.
 * @param buffer The response buffer.
 * @return The parsed messages.
 */
std::vector<Message> parse_list(const ResponseBuffer& buffer)
{
    std::vector<Message> messages;
    const char* ptr = buffer.data.c_str();

    while ((ptr = std::strstr(ptr, "+CMGL:")) != nullptr) {
        ptr += 6;
        Message msg;
        int index = -1;

        if (std::sscanf(ptr, "%d", &index) == 1) {
            msg.index = static_cast<std::int16_t>(index);
        }

        const char* line = std::strstr(ptr, "\r\n");

        if (line == nullptr) {
            break;
        }

        ptr = line + 2;

        const char* end = std::strstr(ptr, "\r\n");
        const std::string_view pdu =
            end ? std::string_view{ptr, static_cast<std::size_t>(end - ptr)} : std::string_view{ptr};
        const std::int16_t saved_index = msg.index;
        if (!PduCodec::decode(pdu, msg)) {
            msg.index = saved_index;
            messages.push_back(std::move(msg));
        }

        if (end) {
            ptr = end;
        } else {
            break;
        }
    }

    return PduCodec::reassemble_messages(std::move(messages));
}

} // namespace comettextel::detail::modem::cmgl

namespace comettextel {

/**
 * @brief Sends a message.
 * @param message The message to send.
 * @param bytes_written The number of bytes written.
 * @param timeout The timeout duration.
 * @return The error code.
 */
std::error_code GsmModem::send_message(const Message& message,
                                       std::size_t* bytes_written,
                                       std::chrono::milliseconds timeout)
{
    std::lock_guard lock(mutex_);

    if (!port_->is_open()) {
        return make_error_code(Errc::NotOpen);
    }

    std::vector<std::string> pdus;
    if (auto ec = PduCodec::encode_segments(message, pdus); ec) {
        return ec;
    }

    std::size_t total_written = 0;
    for (std::string& pdu_hex : pdus) {
        std::size_t written = 0;
        if (auto ec = send_encoded_pdu(std::move(pdu_hex), &written, timeout); ec) {
            return ec;
        }
        total_written += written;
    }

    if (bytes_written) {
        *bytes_written = total_written;
    }

    return {};
}

/**
 * @brief Sends an encoded PDU.
 * @param pdu_hex The PDU hex string to send.
 * @param bytes_written The number of bytes written.
 * @param timeout The timeout duration.
 * @return The error code.
 */
std::error_code GsmModem::send_encoded_pdu(std::string pdu_hex,
                                           std::size_t* bytes_written,
                                           std::chrono::milliseconds timeout)
{
    pdu_hex.push_back(static_cast<char>(0x1A)); // Ctrl-Z

    std::uint8_t smsc_len = 0;
    {
        std::vector<std::uint8_t> first;

        if (auto ec = PduCodec::hex_to_bytes(std::string_view{pdu_hex}.substr(0, 2), first); ec) {
            return ec;
        }

        smsc_len = first.empty() ? std::uint8_t{0} : first[0];
    }

    ++smsc_len; // include the length byte itself

    const std::size_t hex_only = pdu_hex.size() - 1;
    const int cmgs_len = static_cast<int>(hex_only / 2) - static_cast<int>(smsc_len);

    char cmd[32];
    std::snprintf(cmd, sizeof(cmd), "AT+CMGS=%d\r", cmgs_len);

    if (auto ec = write_string(cmd); ec) {
        return ec;
    }

    if (auto ec = expect_prompt(std::chrono::seconds(5)); ec) {
        return ec;
    }

    std::size_t written = 0;

    if (auto ec = port_->write(pdu_hex, &written); ec) {
        return ec;
    }

    if (bytes_written) {
        *bytes_written = written;
    }

    ResponseBuffer buffer;

    if (auto ec = wait_until_ok_unlocked(buffer, timeout, std::chrono::milliseconds(50)); ec) {
        return ec;
    }

    return {};
}

/**
 * @brief Requests a message list.
 * @return The error code.
 */
std::error_code GsmModem::request_message_list()
{
    std::lock_guard lock(mutex_);
    return write_string("AT+CMGL\r");
}

/**
 * @brief Deletes a message.
 * @param index The index of the message to delete.
 * @return The error code.
 */
std::error_code GsmModem::delete_message(int index)
{
    std::lock_guard lock(mutex_);

    if (index < 0) {
        return make_error_code(Errc::InvalidArgument);
    }

    char cmd[32];
    std::snprintf(cmd, sizeof(cmd), "AT+CMGD=%d\r", index);

    return write_string(cmd);
}

/**
 * @brief Parses a message list.
 * @param buffer The response buffer.
 * @return The parsed messages.
 */
std::vector<Message> GsmModem::parse_message_list(const ResponseBuffer& buffer)
{
    return detail::modem::cmgl::parse_list(buffer);
}

} // namespace comettextel
