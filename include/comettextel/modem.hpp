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
#include <deque>
#include <mutex>
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
 *
 * Unsolicited result codes (URCs) such as @c +CDS are demultiplexed out of
 * command responses into an internal Status Report queue. Public methods are
 * internally synchronized; overlapping calls from multiple threads serialize.
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
     * @brief Sends AT, disables echo, switches to PDU mode, and best-effort
     *        configures @c AT+CNMI for status-report URCs.
     * @return Empty error_code on success.
     */
    [[nodiscard]] std::error_code initialize();

    /**
     * @brief Submits an SMS in PDU mode (@c AT+CMGS) and waits for the final result.
     * @param message Message to send.
     * @param bytes_written Optional number of PDU bytes written after the prompt(s).
     * @param timeout Maximum time to wait for OK/ERROR after Ctrl-Z (per segment).
     * @return Empty error_code on success.
     *
     * @note Long payloads are split with concat UDH and sent as multiple @c AT+CMGS.
     */
    [[nodiscard]] std::error_code send_message(
        const Message& message,
        std::size_t* bytes_written = nullptr,
        std::chrono::milliseconds timeout = std::chrono::seconds(10));

    /**
     * @brief Sends one AT command and waits for a final OK or ERROR.
     * @param command Command bytes; a trailing @c \\r is appended when missing.
     * @param response Accumulated modem text including the final result.
     * @param timeout Maximum wait for OK/ERROR.
     * @return Empty on OK; @ref Errc::ModemRejected on ERROR; @ref Errc::Timeout otherwise.
     *
     * URCs are demultiplexed during the wait (same as @ref poll_response).
     */
    [[nodiscard]] std::error_code run_at_command(
        std::string_view command,
        ResponseBuffer& response,
        std::chrono::milliseconds timeout = std::chrono::seconds(3));

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
     *
     * @note URCs are stripped from @p buffer and queued when recognized.
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
     * @brief Drains the serial port and returns one queued SMS-STATUS-REPORT.
     * @param out Receives the decoded status report.
     * @param timeout Wait budget after an initial drain; @c 0 is non-blocking.
     * @return Empty on success; @ref Errc::Timeout when none is available;
     *         @ref Errc::NotOpen when the port is closed.
     */
    [[nodiscard]] std::error_code poll_status_report(
        Message& out,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(0));

    /**
     * @brief Number of decoded status reports waiting in the queue.
     * @return Queued report count (does not read the serial port).
     */
    [[nodiscard]] std::size_t pending_status_reports() const;

    /**
     * @brief Removes recognized URCs from @p stream.
     * @param stream Mutable modem text; complete URC blocks are erased.
     * @param out_status_reports Receives decoded SMS-STATUS-REPORT messages.
     *
     * Incomplete trailing URC fragments are left in @p stream. Single-line
     * indications (@c +CMTI, @c +CDSI) and two-line @c +CMT blocks are stripped
     * without decoding. @c +CDS PDU blocks are decoded when valid.
     */
    static void demux_urcs(std::string& stream, std::vector<Message>& out_status_reports);

    /**
     * @brief Parses @c +CMGL lines from a completed response buffer.
     * @param buffer The buffer to parse.
     * @return The message list (complete concat sets are reassembled; see
     *         @ref PduCodec::reassemble_messages).
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
    mutable std::recursive_mutex mutex_{};
    std::deque<Message> status_reports_{};

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

    /**
     * @brief Sends one already-encoded PDU hex string (@c AT+CMGS).
     * @param pdu_hex Hex TPDU (no Ctrl-Z); Ctrl-Z is appended.
     * @param bytes_written Optional bytes written after the prompt.
     * @param timeout Wait for OK/ERROR after Ctrl-Z.
     * @return Empty error_code on success.
     */
    [[nodiscard]] std::error_code send_encoded_pdu(
        std::string pdu_hex,
        std::size_t* bytes_written,
        std::chrono::milliseconds timeout);

    /**
     * @brief Best-effort @c AT+CNMI configuration for delivery reports.
     */
    void configure_status_report_urc();

    /**
     * @brief Demux URCs from @p buffer into @ref status_reports_.
     * @param buffer Command-response accumulator.
     */
    void demux_into_queue(ResponseBuffer& buffer);

    /**
     * @brief @ref poll_response implementation (caller holds @ref mutex_).
     */
    [[nodiscard]] ModemResponse poll_response_unlocked(ResponseBuffer& buffer);

    /**
     * @brief @ref wait_for_response implementation (caller holds @ref mutex_).
     */
    [[nodiscard]] ModemResponse wait_for_response_unlocked(
        ResponseBuffer& buffer,
        std::chrono::milliseconds timeout,
        std::chrono::milliseconds poll_interval);

    /**
     * @brief @ref wait_until_ok implementation (caller holds @ref mutex_).
     */
    [[nodiscard]] std::error_code wait_until_ok_unlocked(
        ResponseBuffer& buffer,
        std::chrono::milliseconds timeout,
        std::chrono::milliseconds poll_interval);
};

} // namespace comettextel
