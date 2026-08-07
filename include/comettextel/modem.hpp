/**
 * @file modem.hpp
 * @brief GSM modem control via AT commands over a serial port.
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @email jiowcl@gmail.com
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#pragma once

#include <cstddef>
#include <string>
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
     * @brief Submits one SMS in PDU mode (@c AT+CMGS).
     * @param message Message to send.
     * @param bytes_written Optional number of PDU bytes written after the prompt.
     */
    [[nodiscard]] std::error_code send_message(const Message& message,
                                               std::size_t* bytes_written = nullptr);

    /**
     * @brief Requests the full message list (@c AT+CMGL).
     * @note Call @ref poll_response repeatedly, then @ref parse_message_list.
     */
    [[nodiscard]] std::error_code request_message_list();

    /**
     * @brief Deletes a stored message by index (@c AT+CMGD).
     */
    [[nodiscard]] std::error_code delete_message(int index);

    /**
     * @brief Appends freshly received serial data to @p buffer and classifies status.
     */
    [[nodiscard]] ModemResponse poll_response(ResponseBuffer& buffer);

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
};

} // namespace comettextel
