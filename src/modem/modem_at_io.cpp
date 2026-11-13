/**
 * @file modem_at_io.cpp
 * @brief Serial read/write, response polling, and CMGS prompt wait.
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#include "comettextel/modem.hpp"

#include "modem/detail/at_response.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace comettextel::detail::modem::at {

/**
 * @brief Checks if a string ends with a suffix.
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
 * @brief Checks if a string contains a token.
 * @param text The string to check.
 * @param token The token to check for.
 * @return True if the string contains the token, false otherwise.
 */
[[nodiscard]] bool contains_token(std::string_view text, std::string_view token)
{
    return text.find(token) != std::string_view::npos;
}

/**
 * @brief Classifies accumulated modem text (no serial I/O).
 * @param data The modem text to classify.
 * @return The classification result.
 */
[[nodiscard]] ModemResponse classify_response(std::string_view data)
{
    if (data.empty()) {
        return ModemResponse::Wait;
    }

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

} // namespace comettextel::detail::modem::at

namespace comettextel {

/**
 * @brief Classifies accumulated modem text (no serial I/O).
 * @param data The modem text to classify.
 * @return The classification result.
 */
ModemResponse GsmModem::classify_response(std::string_view data)
{
    return detail::modem::at::classify_response(data);
}

/**
 * @brief Writes a string to the serial port.
 * @param text The string to write.
 * @return The error code.
 */
std::error_code GsmModem::write_string(std::string_view text)
{
    return port_->write(text);
}

/**
 * @brief Reads a string from the serial port.
 * @param max_bytes The maximum number of bytes to read.
 * @param out The output string.
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

/**
 * @brief Waits for the prompt from the modem.
 * @param timeout The timeout duration.
 * @return The error code.
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
            demux_into_queue(buffer);
        }

        if (detail::modem::at::contains_token(buffer.data, ">") ||
            classify_response(buffer.data) == ModemResponse::Error) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    demux_into_queue(buffer);

    if (classify_response(buffer.data) == ModemResponse::Error) {
        return make_error_code(Errc::ModemRejected);
    }

    if (buffer.data.find('>') == std::string::npos) {
        return make_error_code(Errc::ModemRejected);
    }

    return {};
}

/**
 * @brief Polls a response from the modem.
 * @param buffer The buffer to store the response in.
 * @return The classification result.
 */
ModemResponse GsmModem::poll_response_unlocked(ResponseBuffer& buffer)
{
    std::string chunk;

    if (auto ec = read_string(256, chunk); ec) {
        demux_into_queue(buffer);
        return classify_response(buffer.data);
    }

    if (!chunk.empty()) {
        buffer.data.append(chunk);
    }

    demux_into_queue(buffer);
    return classify_response(buffer.data);
}

/**
 * @brief Polls a response from the modem.
 * @param buffer The buffer to store the response in.
 * @return The classification result.
 */
ModemResponse GsmModem::poll_response(ResponseBuffer& buffer)
{
    std::lock_guard lock(mutex_);
    return poll_response_unlocked(buffer);
}

/**
 * @brief Waits for a response from the modem.
 * @param buffer The buffer to store the response in.
 * @param timeout The timeout duration.
 * @param poll_interval The poll interval.
 * @return The classification result.
 */
ModemResponse GsmModem::wait_for_response_unlocked(ResponseBuffer& buffer,
                                                   std::chrono::milliseconds timeout,
                                                   std::chrono::milliseconds poll_interval)
{
    if (timeout.count() < 0) {
        return ModemResponse::Wait;
    }

    if (poll_interval.count() <= 0) {
        poll_interval = std::chrono::milliseconds(1);
    }

    demux_into_queue(buffer);
    if (const auto existing = classify_response(buffer.data); existing != ModemResponse::Wait) {
        return existing;
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
        const auto status = poll_response_unlocked(buffer);
        if (status != ModemResponse::Wait) {
            return status;
        }
        std::this_thread::sleep_for(poll_interval);
    }

    return poll_response_unlocked(buffer);
}

/**
 * @brief Waits for a response from the modem.
 * @param buffer The buffer to store the response in.
 * @param timeout The timeout duration.
 * @param poll_interval The poll interval.
 * @return The classification result.
 */
ModemResponse GsmModem::wait_for_response(ResponseBuffer& buffer,
                                          std::chrono::milliseconds timeout,
                                          std::chrono::milliseconds poll_interval)
{
    std::lock_guard lock(mutex_);
    return wait_for_response_unlocked(buffer, timeout, poll_interval);
}

/**
 * @brief Waits for a response from the modem.
 * @param buffer The buffer to store the response in.
 * @param timeout The timeout duration.
 * @param poll_interval The poll interval.
 * @return The error code.
 */
std::error_code GsmModem::wait_until_ok_unlocked(ResponseBuffer& buffer,
                                                   std::chrono::milliseconds timeout,
                                                   std::chrono::milliseconds poll_interval)
{
    switch (wait_for_response_unlocked(buffer, timeout, poll_interval)) {
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
 * @brief Waits for a response from the modem.
 * @param buffer The buffer to store the response in.
 * @param timeout The timeout duration.
 * @param poll_interval The poll interval.
 * @return The error code.
 */
std::error_code GsmModem::wait_until_ok(ResponseBuffer& buffer,
                                        std::chrono::milliseconds timeout,
                                        std::chrono::milliseconds poll_interval)
{
    std::lock_guard lock(mutex_);
    return wait_until_ok_unlocked(buffer, timeout, poll_interval);
}

} // namespace comettextel
