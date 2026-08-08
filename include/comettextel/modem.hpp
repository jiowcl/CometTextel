/**
 * @file modem.hpp
 * @brief GSM modem control via AT commands over a serial port.
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @email jiowcl@gmail.com
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "comettextel/export.hpp"
#include "comettextel/serial.hpp"
#include "comettextel/types.hpp"

namespace comettextel {

/**
 * @brief High-level GSM modem helper (PDU mode).
 *
 * Owns no serial handle by default; callers pass an already-opened @ref SerialPort
 * or let @ref open_and_init create one.
 */
class COMETTEXTEL_API GsmModem {
public:
    /**
     * @brief Constructs a modem that owns an internal serial port.
     */
    GsmModem();

    /**
     * @brief Constructs a modem helper bound to an existing serial port.
     * @param port Reference must outlive this object.
     */
    explicit GsmModem(SerialPort& port);

    /**
     * @brief Opens @p device on the bound port and runs initialization.
     * @param device The device to open.
     * @param config The serial configuration.
     * @return Empty error_code on success.
     */
    [[nodiscard]] std::error_code open_and_init(std::string_view device,
                                                const SerialConfig& config = {});

    /**
     * @brief Sends AT, disables echo, and switches to PDU mode (@c AT+CMGF=0).
     * @return Empty error_code on success.
     */
    [[nodiscard]] std::error_code initialize();

    /**
     * @brief Submits one SMS in PDU mode (@c AT+CMGS) and waits for the final result.
     * @param message Message to send.
     * @param bytes_written Optional number of PDU bytes written after the prompt.
     * @param timeout Maximum time to wait for the final OK/ERROR after Ctrl-Z.
     * @return Empty error_code on success.
     */
    [[nodiscard]] std::error_code send_message(
        const Message& message,
        std::size_t* bytes_written = nullptr,
        std::chrono::milliseconds timeout = std::chrono::seconds(10));

    /**
     * @brief Requests the full message list (@c AT+CMGL).
     * @return Empty error_code on success.
     * @note Prefer @ref wait_for_response / @ref wait_until_ok, then @ref parse_message_list.
     */
    [[nodiscard]] std::error_code request_message_list();

    /**
     * @brief Deletes a stored message by index (@c AT+CMGD).
     * @param index The index of the message to delete.
     * @return Empty error_code on success.
     * @note Call @ref wait_until_ok afterwards to confirm completion.
     */
    [[nodiscard]] std::error_code delete_message(int index);

    /**
     * @brief Classifies accumulated modem text without reading the serial port.
     * @param data The data to classify.
     * @return The modem response.
     *
     * Recognizes final result codes such as @c OK, @c ERROR, @c +CMS ERROR, and
     * @c +CME ERROR (including common @c \\r\\n framing variants).
     */
    [[nodiscard]] static ModemResponse classify_response(std::string_view data);

    /**
     * @brief Appends freshly received serial data to @p buffer and classifies status.
     * @param buffer The buffer to classify.
     * @return The modem response.
     */
    [[nodiscard]] ModemResponse poll_response(ResponseBuffer& buffer);

    /**
     * @brief Polls until OK/ERROR or @p timeout elapses.
     * @param buffer The buffer to classify.
     * @param timeout The timeout.
     * @param poll_interval The poll interval.
     * @return @ref ModemResponse::Ok, @ref ModemResponse::Error, or
     *         @ref ModemResponse::Wait on timeout.
     */
    [[nodiscard]] ModemResponse wait_for_response(
        ResponseBuffer& buffer,
        std::chrono::milliseconds timeout = std::chrono::seconds(5),
        std::chrono::milliseconds poll_interval = std::chrono::milliseconds(50));

    /**
     * @brief Convenience wrapper around @ref wait_for_response.
     * @param buffer The buffer to classify.
     * @param timeout The timeout.
     * @param poll_interval The poll interval.
     * @return Empty on OK; @ref Errc::ModemRejected on ERROR; @ref Errc::Timeout otherwise.
     */
    [[nodiscard]] std::error_code wait_until_ok(
        ResponseBuffer& buffer,
        std::chrono::milliseconds timeout = std::chrono::seconds(5),
        std::chrono::milliseconds poll_interval = std::chrono::milliseconds(50));

    /**
     * @brief Parses @c +CMGL lines from a completed response buffer.
     * @param buffer The buffer to parse.
     * @return The message list.
     */
    [[nodiscard]] static std::vector<Message> parse_message_list(const ResponseBuffer& buffer);

    /**
     * @brief Returns the bound serial port.
     * @return The bound serial port.
     */
    [[nodiscard]] SerialPort& port() noexcept;

    /**
     * @brief Returns the bound serial port (const).
     * @return The bound serial port (const).
     */
    [[nodiscard]] const SerialPort& port() const noexcept;

private:
    SerialPort* port_;
    SerialPort owned_port_{};
    bool owns_port_{false};

    /**
     * @brief Writes a string to the modem.
     * @param text The string to write.
     * @return Empty error_code on success.
     */
    [[nodiscard]] std::error_code write_string(std::string_view text);

    /**
     * @brief Reads a string from the modem.
     * @param max_bytes The maximum number of bytes to read.
     * @param out The string to store the read data.
     * @return Empty error_code on success.
     */
    [[nodiscard]] std::error_code read_string(std::size_t max_bytes, std::string& out);

    /**
     * @brief Waits for the prompt.
     * @param timeout The timeout.
     * @return Empty error_code on success.
     */
    [[nodiscard]] std::error_code expect_prompt(std::chrono::milliseconds timeout);
};

} // namespace comettextel
