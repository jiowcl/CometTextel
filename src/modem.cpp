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
#include <thread>

namespace comettextel {
namespace {

/**
 * @brief Checks if a string ends with a given suffix.
 * @param text The string to check.
 * @param suffix The suffix to check for.
 * @return True if the string ends with the suffix, false otherwise.
 */
[[nodiscard]] bool ends_with(std::string_view text, std::string_view suffix)
{
    return text.size() >= suffix.size() &&
           text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

/**
 * @brief Checks if a string contains a given token.
 * @param text The string to check.
 * @param token The token to check for.
 * @return True if the string contains the token, false otherwise.
 */
[[nodiscard]] bool contains_token(std::string_view text, std::string_view token)
{
    return text.find(token) != std::string_view::npos;
}

} // namespace

/**
 * @brief Constructs a GsmModem that owns an internal serial port.
 */
GsmModem::GsmModem()
    : port_(&owned_port_)
    , owns_port_(true)
{
}

/**
 * @brief Constructs a GsmModem that owns an existing serial port.
 * @param port The serial port to use.
 */
GsmModem::GsmModem(SerialPort& port)
    : port_(&port)
    , owns_port_(false)
{
}

/**
 * @brief Opens the serial port and initializes the modem.
 * @param device The device to open.
 * @param config The serial configuration to use.
 * @return An error code if the operation failed.
 */
std::error_code GsmModem::open_and_init(std::string_view device, const SerialConfig& config)
{
    if (auto ec = port_->open(device, config); ec) {
        return ec;
    }

    return initialize();
}

/**
 * @brief Initializes the modem.
 * @return An error code if the operation failed.
 */
std::error_code GsmModem::initialize()
{
    if (!port_->is_open()) {
        return make_error_code(Errc::NotOpen);
    }

    ResponseBuffer buffer;

    if (auto ec = write_string("AT\r"); ec) {
        return ec;
    }

    buffer.data.clear();
    if (auto ec = wait_until_ok(buffer, std::chrono::seconds(3)); ec) {
        return ec;
    }

    if (auto ec = write_string("ATE0\r"); ec) {
        return ec;
    }

    buffer.data.clear();

    if (auto ec = wait_until_ok(buffer, std::chrono::seconds(3)); ec) {
        return ec;
    }

    if (auto ec = write_string("AT+CMGF=0\r"); ec) {
        return ec;
    }

    buffer.data.clear();

    if (auto ec = wait_until_ok(buffer, std::chrono::seconds(3)); ec) {
        return ec;
    }

    return {};
}

/**
 * @brief Waits for the modem to respond with a prompt.
 * @param timeout The timeout duration.
 * @return An error code if the operation failed.
 */
std::error_code GsmModem::expect_prompt(std::chrono::milliseconds timeout)
{
    ResponseBuffer buffer;
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
        std::string chunk;

        if (auto ec = read_string(128, chunk); ec) {
            return ec;
        }

        if (!chunk.empty()) {
            buffer.data.append(chunk);
        }

        if (contains_token(buffer.data, ">") ||
            classify_response(buffer.data) == ModemResponse::Error) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    if (classify_response(buffer.data) == ModemResponse::Error) {
        return make_error_code(Errc::ModemRejected);
    }

    if (buffer.data.find('>') == std::string::npos) {
        return make_error_code(Errc::ModemRejected);
    }

    return {};
}

/**
 * @brief Sends a message to the modem.
 * @param message The message to send.
 * @param bytes_written The number of bytes written.
 * @param timeout The timeout duration.
 * @return An error code if the operation failed.
 */
std::error_code GsmModem::send_message(const Message& message,
                                       std::size_t* bytes_written,
                                       std::chrono::milliseconds timeout)
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

    if (auto ec = wait_until_ok(buffer, timeout); ec) {
        return ec;
    }

    return {};
}

/**
 * @brief Requests the message list from the modem.
 * @return An error code if the operation failed.
 */
std::error_code GsmModem::request_message_list()
{
    return write_string("AT+CMGL\r");
}

/**
 * @brief Deletes a message from the modem.
 * @param index The index of the message to delete.
 * @return An error code if the operation failed.
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
 * @brief Classifies a response from the modem.
 * @param data The data to classify.
 * @return The modem response.
 */
ModemResponse GsmModem::classify_response(std::string_view data)
{
    if (data.empty()) {
        return ModemResponse::Wait;
    }

    // Prefer final-result framing; also accept bare suffixes from some modems.
    if (contains_token(data, "\r\nOK\r\n") || ends_with(data, "OK\r\n") || ends_with(data, "\nOK\n") ||
        ends_with(data, "OK\n") || ends_with(data, "OK\r")) {
        return ModemResponse::Ok;
    }

    if (contains_token(data, "+CMS ERROR") || contains_token(data, "+CME ERROR") ||
        contains_token(data, "\r\nERROR\r\n") || ends_with(data, "ERROR\r\n") ||
        ends_with(data, "ERROR\n") || ends_with(data, "ERROR\r")) {
        return ModemResponse::Error;
    }

    return ModemResponse::Wait;
}

/**
 * @brief Polls the modem for a response.
 * @param buffer The buffer to store the response.
 * @return The modem response.
 */
ModemResponse GsmModem::poll_response(ResponseBuffer& buffer)
{
    std::string chunk;

    if (auto ec = read_string(256, chunk); ec) {
        // Soft serial timeouts usually return success with empty data.
        // Hard failures are treated as "keep waiting" so callers can time out.
        return ModemResponse::Wait;
    }

    if (!chunk.empty()) {
        buffer.data.append(chunk);
    }

    return classify_response(buffer.data);
}

/**
 * @brief Waits for a response from the modem.
 * @param buffer The buffer to store the response.
 * @param timeout The timeout duration.
 * @param poll_interval The poll interval.
 * @return The modem response.
 */
ModemResponse GsmModem::wait_for_response(ResponseBuffer& buffer,
                                         std::chrono::milliseconds timeout,
                                         std::chrono::milliseconds poll_interval)
{
    if (timeout.count() < 0) {
        return ModemResponse::Wait;
    }

    if (poll_interval.count() <= 0) {
        poll_interval = std::chrono::milliseconds(1);
    }

    // Already complete?
    if (const auto existing = classify_response(buffer.data); existing != ModemResponse::Wait) {
        return existing;
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
        const auto status = poll_response(buffer);
        if (status != ModemResponse::Wait) {
            return status;
        }
        std::this_thread::sleep_for(poll_interval);
    }

    // Final drain attempt before giving up.
    return poll_response(buffer);
}

/**
 * @brief Waits for the modem to respond with an OK.
 * @param buffer The buffer to store the response.
 * @param timeout The timeout duration.
 * @param poll_interval The poll interval.
 * @return An error code if the operation failed.
 */
std::error_code GsmModem::wait_until_ok(ResponseBuffer& buffer,
                                        std::chrono::milliseconds timeout,
                                        std::chrono::milliseconds poll_interval)
{
    switch (wait_for_response(buffer, timeout, poll_interval)) {
    case ModemResponse::Ok:
        return {};
    case ModemResponse::Error:
        return make_error_code(Errc::ModemRejected);
    case ModemResponse::Wait:
    default:
        return make_error_code(Errc::Timeout);
    }
}

/**
 * @brief Parses the message list from the modem.
 * @param buffer The buffer to store the response.
 * @return The message list.
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

/**
 * @brief Writes a string to the modem.
 * @param text The string to write.
 * @return An error code if the operation failed.
 */
std::error_code GsmModem::write_string(std::string_view text)
{
    return port_->write(text);
}

/**
 * @brief Reads a string from the modem.
 * @param max_bytes The maximum number of bytes to read.
 * @param out The string to store the read data.
 * @return An error code if the operation failed.
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
