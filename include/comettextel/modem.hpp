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
     */
    [[nodiscard]] std::error_code open_and_init(std::string_view device,
                                                const SerialConfig& config = {});

    /**
     * @brief Sends AT, disables echo, and switches to PDU mode (@c AT+CMGF=0).
     */
    [[nodiscard]] std::error_code initialize();

    /**
     * @brief Submits one SMS in PDU mode (@c AT+CMGS) and waits for the final result.
     * @param message Message to send.
     * @param bytes_written Optional number of PDU bytes written after the prompt.
     * @param timeout Maximum time to wait for the final OK/ERROR after Ctrl-Z.
     */
    [[nodiscard]] std::error_code send_message(
        const Message& message,
        std::size_t* bytes_written = nullptr,
        std::chrono::milliseconds timeout = std::chrono::seconds(10));

    /**
     * @brief Requests the full message list (@c AT+CMGL).
     * @note Prefer @ref wait_for_response / @ref wait_until_ok, then @ref parse_message_list.
     */
    [[nodiscard]] std::error_code request_message_list();

    /**
     * @brief Deletes a stored message by index (@c AT+CMGD).
     * @note Call @ref wait_until_ok afterwards to confirm completion.
     */
    [[nodiscard]] std::error_code delete_message(int index);

    /**
     * @brief Classifies accumulated modem text without reading the serial port.
     *
     * Recognizes final result codes such as @c OK, @c ERROR, @c +CMS ERROR, and
     * @c +CME ERROR (including common @c \\r\\n framing variants).
     */
    [[nodiscard]] static ModemResponse classify_response(std::string_view data);

    /**
     * @brief Appends freshly received serial data to @p buffer and classifies status.
     */
    [[nodiscard]] ModemResponse poll_response(ResponseBuffer& buffer);

    /**
     * @brief Polls until OK/ERROR or @p timeout elapses.
     * @return @ref ModemResponse::Ok, @ref ModemResponse::Error, or
     *         @ref ModemResponse::Wait on timeout.
     */
    [[nodiscard]] ModemResponse wait_for_response(
        ResponseBuffer& buffer,
        std::chrono::milliseconds timeout = std::chrono::seconds(5),
        std::chrono::milliseconds poll_interval = std::chrono::milliseconds(50));

    /**
     * @brief Convenience wrapper around @ref wait_for_response.
     * @return Empty on OK; @ref Errc::ModemRejected on ERROR; @ref Errc::Timeout otherwise.
     */
    [[nodiscard]] std::error_code wait_until_ok(
        ResponseBuffer& buffer,
        std::chrono::milliseconds timeout = std::chrono::seconds(5),
        std::chrono::milliseconds poll_interval = std::chrono::milliseconds(50));

    /**
     * @brief Parses @c +CMGL lines from a completed response buffer.
     */
    [[nodiscard]] static std::vector<Message> parse_message_list(const ResponseBuffer& buffer);

    /**
     * @brief Returns the bound serial port.
     */
    [[nodiscard]] SerialPort& port() noexcept;

    /**
     * @brief Returns the bound serial port (const).
     */
    [[nodiscard]] const SerialPort& port() const noexcept;

private:
    SerialPort* port_;
    SerialPort owned_port_{};
    bool owns_port_{false};

    [[nodiscard]] std::error_code write_string(std::string_view text);
    [[nodiscard]] std::error_code read_string(std::size_t max_bytes, std::string& out);
    [[nodiscard]] std::error_code expect_prompt(std::chrono::milliseconds timeout);
};

} // namespace comettextel
