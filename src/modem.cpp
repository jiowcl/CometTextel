/**
 * @file modem.cpp
 * @brief GSM modem AT-command helper implementation.
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @email jiowcl@gmail.com
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#include "comettextel/modem.hpp"

#include "comettextel/pdu.hpp"

#include <cstdio>
#include <cstring>

namespace comettextel {

/**
 * @brief Construct a new GsmModem object.
 */
GsmModem::GsmModem()
    : port_(&owned_port_)
    , owns_port_(true)
{
}

/**
 * @brief Construct a new GsmModem object.
 * @param port The serial port to use.
 */
GsmModem::GsmModem(SerialPort& port)
    : port_(&port)
    , owns_port_(false)
{
}

/**
 * @brief Open the modem and initialize it.
 * @param device The device to open.
 * @param config The serial configuration.
 * @return The error code.
 */
std::error_code GsmModem::open_and_init(std::string_view device, const SerialConfig& config)
{
    if (auto ec = port_->open(device, config); ec) {
        return ec;
    }

    return initialize();
}

/**
 * @brief Initialize the modem.
 * @return The error code.
 */
std::error_code GsmModem::initialize()
{
    if (!port_->is_open()) {
        return make_error_code(Errc::NotOpen);
    }

    std::string answer;

    if (auto ec = write_string("AT\r"); ec) {
        return ec;
    }

    if (auto ec = read_string(128, answer); ec) {
        return ec;
    }

    if (answer.find("OK") == std::string::npos) {
        return make_error_code(Errc::ModemRejected);
    }

    if (auto ec = write_string("ATE0\r"); ec) {
        return ec;
    }

    if (auto ec = read_string(128, answer); ec) {
        return ec;
    }

    if (auto ec = write_string("AT+CMGF=0\r"); ec) {
        return ec;
    }

    if (auto ec = read_string(128, answer); ec) {
        return ec;
    }

    if (answer.find("OK") == std::string::npos) {
        return make_error_code(Errc::ModemRejected);
    }

    return {};
}

/**
 * @brief Send a message to the modem.
 * @param message The message to send.
 * @param bytes_written The number of bytes written.
 * @return The error code.
 */
std::error_code GsmModem::send_message(const Message& message, std::size_t* bytes_written)
{
    if (!port_->is_open()) {
        return make_error_code(Errc::NotOpen);
    }

    std::string pdu_hex;

    if (auto ec = PduCodec::encode(message, pdu_hex); ec) {
        return ec;
    }

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

    // pdu_hex includes a trailing Ctrl-Z which is not part of the hex PDU.
    const std::size_t hex_only = pdu_hex.size() - 1;
    const int cmgs_len = static_cast<int>(hex_only / 2) - static_cast<int>(smsc_len);

    char cmd[32];
    std::snprintf(cmd, sizeof(cmd), "AT+CMGS=%d\r", cmgs_len);

    if (auto ec = write_string(cmd); ec) {
        return ec;
    }

    std::string answer;

    if (auto ec = read_string(128, answer); ec) {
        return ec;
    }

    if (answer.size() < 4 || answer.find("\r\n> ") == std::string::npos) {
        // Some modems emit ">" without the exact historic 4-byte pattern.
        if (answer.find('>') == std::string::npos) {
            return make_error_code(Errc::ModemRejected);
        }
    }

    std::size_t written = 0;

    if (auto ec = port_->write(pdu_hex, &written); ec) {
        return ec;
    }

    if (bytes_written) {
        *bytes_written = written;
    }

    return {};
}

/**
 * @brief Request the message list from the modem.
 * @return The error code.
 */
std::error_code GsmModem::request_message_list()
{
    return write_string("AT+CMGL\r");
}

/**
 * @brief Delete a message from the modem.
 * @param index The index of the message to delete.
 * @return The error code.
 */
std::error_code GsmModem::delete_message(int index)
{
    if (index < 0) {
        return make_error_code(Errc::InvalidArgument);
    }

    char cmd[32];
    std::snprintf(cmd, sizeof(cmd), "AT+CMGD=%d\r", index);

    return write_string(cmd);
}

/**
 * @brief Poll the response from the modem.
 * @param buffer The response buffer.
 * @return The response.
 */
ModemResponse GsmModem::poll_response(ResponseBuffer& buffer)
{
    std::string chunk;

    if (read_string(128, chunk)) {
        return ModemResponse::Wait;
    }

    if (!chunk.empty()) {
        buffer.data.append(chunk);
    }

    if (buffer.data.size() >= 4) {
        if (buffer.data.size() >= 4 &&
            buffer.data.compare(buffer.data.size() - 4, 4, "OK\r\n") == 0) {
            return ModemResponse::Ok;
        }

        if (buffer.data.find("+CMS ERROR") != std::string::npos ||
            buffer.data.find("ERROR") != std::string::npos) {
            return ModemResponse::Error;
        }
    }

    return ModemResponse::Wait;
}

/**
 * @brief Parse the message list from the response buffer.
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

        // PDU hex runs until CR/LF.
        const char* end = std::strstr(ptr, "\r\n");
        std::string_view pdu = end ? std::string_view{ptr, static_cast<std::size_t>(end - ptr)}
                                   : std::string_view{ptr};
        if (!PduCodec::decode(pdu, msg)) {
            messages.push_back(std::move(msg));
        }

        if (end) {
            ptr = end;
        } else {
            break;
        }
    }

    return messages;
}

/**
 * @brief Get the serial port of the modem.
 * @return The serial port of the modem.
 */
SerialPort& GsmModem::port() noexcept
{
    return *port_;
}

/**
 * @brief Get the serial port of the modem.
 * @return The serial port of the modem.
 */
const SerialPort& GsmModem::port() const noexcept
{
    return *port_;
}

/**
 * @brief Write a string to the modem.
 * @param text The string to write.
 * @return The error code.
 */
std::error_code GsmModem::write_string(std::string_view text)
{
    return port_->write(text);
}

/**
 * @brief Read a string from the modem.
 * @param max_bytes The maximum number of bytes to read.
 * @param out The string to read the data into.
 * @return The error code.
 */
std::error_code GsmModem::read_string(std::size_t max_bytes, std::string& out)
{
    std::vector<std::byte> raw;

    if (auto ec = port_->read(max_bytes, raw); ec) {
        return ec;
    }

    out.assign(reinterpret_cast<const char*>(raw.data()), raw.size());

    return {};
}

} // namespace comettextel
