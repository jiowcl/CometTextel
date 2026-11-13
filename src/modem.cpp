/**
 * @file modem.cpp
 * @brief GSM modem high-level AT commands (I/O in modem/modem_at_io.cpp, URC in modem_urc.cpp).
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @email jiowcl@gmail.com
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#include "comettextel/modem.hpp"
#include "comettextel/pdu.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace comettextel {

/**
 * @brief Constructs a new GsmModem object with an owned serial port.
 */
GsmModem::GsmModem()
    : port_(&owned_port_)
    , owns_port_(true)
{
}

/**
 * @brief Constructs a new GsmModem object with a shared serial port.
 * @param port The serial port to use.
 */
GsmModem::GsmModem(SerialPort& port)
    : port_(&port)
    , owns_port_(false)
{
}

/**
 * @brief Opens and initializes the modem.
 * @param device The device to open.
 * @param config The serial configuration.
 * @return The error code.
 */
std::error_code GsmModem::open_and_init(std::string_view device, const SerialConfig& config)
{
    std::lock_guard lock(mutex_);

    if (auto ec = port_->open(device, config); ec) {
        return ec;
    }

    return initialize();
}

/**
 * @brief Initializes the modem.
 * @return The error code.
 */
std::error_code GsmModem::initialize()
{
    std::lock_guard lock(mutex_);

    if (!port_->is_open()) {
        return make_error_code(Errc::NotOpen);
    }

    ResponseBuffer buffer;

    if (auto ec = write_string("AT\r"); ec) {
        return ec;
    }

    buffer.data.clear();
    if (auto ec = wait_until_ok_unlocked(buffer, std::chrono::seconds(3),
                                         std::chrono::milliseconds(50));
        ec) {
        return ec;
    }

    if (auto ec = write_string("ATE0\r"); ec) {
        return ec;
    }

    buffer.data.clear();
    if (auto ec = wait_until_ok_unlocked(buffer, std::chrono::seconds(3),
                                         std::chrono::milliseconds(50));
        ec) {
        return ec;
    }

    if (auto ec = write_string("AT+CMGF=0\r"); ec) {
        return ec;
    }

    buffer.data.clear();
    if (auto ec = wait_until_ok_unlocked(buffer, std::chrono::seconds(3),
                                         std::chrono::milliseconds(50));
        ec) {
        return ec;
    }

    configure_status_report_urc();
    return {};
}

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
 * @brief Runs an AT command.
 * @param command The command to run.
 * @param response The response buffer.
 * @param timeout The timeout duration.
 * @return The error code.
 */
std::error_code GsmModem::run_at_command(std::string_view command,
                                         ResponseBuffer& response,
                                         std::chrono::milliseconds timeout)
{
    std::lock_guard lock(mutex_);

    if (!port_->is_open()) {
        return make_error_code(Errc::NotOpen);
    }

    std::string cmd(command);
    if (cmd.empty() || (cmd.back() != '\r' && cmd.back() != '\n')) {
        cmd.push_back('\r');
    }

    response.data.clear();
    if (auto ec = write_string(cmd); ec) {
        return ec;
    }

    return wait_until_ok_unlocked(response, timeout, std::chrono::milliseconds(50));
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
        std::string_view pdu = end ? std::string_view{ptr, static_cast<std::size_t>(end - ptr)}
                                   : std::string_view{ptr};
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

/**
 * @brief Gets the serial port.
 * @return The serial port.
 */
SerialPort& GsmModem::port() noexcept
{
    return *port_;
}

/**
 * @brief Gets the serial port.
 * @return The serial port.
 */
const SerialPort& GsmModem::port() const noexcept
{
    return *port_;
}

} // namespace comettextel
