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

#include <cctype>
#include <cstdio>
#include <cstring>
#include <thread>

namespace comettextel {
namespace {

/**
 * @brief Checks if a string ends with a given suffix.
 * @param text The string to check.
 * @param suffix The suffix to check for.
 * @return true if the string ends with the suffix; false otherwise.
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
 * @return true if the string contains the token; false otherwise.
 */
[[nodiscard]] bool contains_token(std::string_view text, std::string_view token)
{
    return text.find(token) != std::string_view::npos;
}

/**
 * @brief Checks if a string starts with a given prefix.
 * @param text The string to check.
 * @param prefix The prefix to check for.
 * @return true if the string starts with the prefix; false otherwise.
 */
[[nodiscard]] bool is_line_start(std::string_view text, std::size_t index)
{
    if (index == 0) {
        return true;
    }
    const char prev = text[index - 1];
    return prev == '\n' || prev == '\r';
}

/**
 * @brief Finds the position of a given tag in a string.
 * @param text The string to search.
 * @param tag The tag to search for.
 * @return The position of the tag; std::string_view::npos if not found.
 */
[[nodiscard]] std::size_t find_urc_tag(std::string_view text, std::string_view tag)
{
    std::size_t pos = 0;
    while ((pos = text.find(tag, pos)) != std::string_view::npos) {
        if (is_line_start(text, pos)) {
            return pos;
        }
        ++pos;
    }
    return std::string_view::npos;
}

/**
 * @brief Skips spaces in a string.
 * @param text The string to search.
 * @param index The index to start searching from.
 * @return The index of the first non-space character.
 */
[[nodiscard]] std::size_t skip_spaces(std::string_view text, std::size_t index)
{
    while (index < text.size() &&
           (text[index] == ' ' || text[index] == '\t')) {
        ++index;
    }
    return index;
}

/**
 * @brief Erases a range of characters in a string.
 * @param stream The string to erase from.
 * @param begin The index to start erasing from.
 * @param end The index to stop erasing at.
 */
void erase_through(std::string& stream, std::size_t begin, std::size_t end)
{
    if (begin > end || end > stream.size()) {
        return;
    }
    stream.erase(begin, end - begin);
}

/**
 * @brief Erase a single CRLF-terminated line starting at @p begin.
 * @return true when a complete line was removed; false when incomplete.
 */
[[nodiscard]] bool erase_single_line(std::string& stream, std::size_t begin)
{
    const std::size_t nl = stream.find("\r\n", begin);
    if (nl == std::string::npos) {
        return false;
    }
    erase_through(stream, begin, nl + 2);
    return true;
}

/**
 * @brief Erase a length-prefixed two-line URC (+CDS: / +CMT:).
 * @param decode_status When true, decode the PDU as SMS-STATUS-REPORT.
 * @return 1 erased, 0 incomplete, -1 not this form (caller may strip as text line).
 */
[[nodiscard]] int erase_length_urc(std::string& stream,
                                   std::size_t begin,
                                   std::string_view tag,
                                   bool decode_status,
                                   std::vector<Message>* out_status_reports)
{
    std::size_t cursor = skip_spaces(stream, begin + tag.size());
    if (cursor >= stream.size() ||
        !std::isdigit(static_cast<unsigned char>(stream[cursor]))) {
        return -1;
    }

    const std::size_t header_nl = stream.find("\r\n", cursor);
    if (header_nl == std::string::npos) {
        return 0;
    }

    const std::size_t pdu_begin = header_nl + 2;
    const std::size_t pdu_nl = stream.find("\r\n", pdu_begin);
    if (pdu_nl == std::string::npos) {
        return 0;
    }

    if (decode_status && out_status_reports != nullptr) {
        const std::string_view pdu{stream.data() + pdu_begin, pdu_nl - pdu_begin};
        Message report;
        if (!PduCodec::decode(pdu, report) && report.is_status_report) {
            out_status_reports->push_back(std::move(report));
        }
    }

    erase_through(stream, begin, pdu_nl + 2);
    return 1;
}

} // namespace

/**
 * @brief Constructs a new GsmModem object.
 */
GsmModem::GsmModem()
    : port_(&owned_port_)
    , owns_port_(true)
{
}

/**
 * @brief Constructs a new GsmModem object with a given serial port.
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
 * @param config The serial port configuration.
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
 * @brief Configures the status report URC.
 */
void GsmModem::configure_status_report_urc()
{
    // Best-effort only: modem CNMI dialects vary. Prefer routing status reports
    // (+CDS) to the TE without failing initialize when unsupported.
    static constexpr const char* kCandidates[] = {
        "AT+CNMI=2,1,0,1,0\r",
        "AT+CNMI=1,1,0,1,0\r",
        "AT+CNMI=2,0,0,1,0\r",
    };

    for (const char* cmd : kCandidates) {
        ResponseBuffer buffer;
        if (write_string(cmd)) {
            continue;
        }
        buffer.data.clear();
        if (!wait_until_ok_unlocked(buffer, std::chrono::seconds(2),
                                    std::chrono::milliseconds(50))) {
            return;
        }
    }
}

/**
 * @brief Waits for the modem to respond with a prompt.
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

        if (contains_token(buffer.data, ">") ||
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
 * @brief Sends a message to the modem.
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
 * @brief Sends an encoded PDU to the modem.
 * @param pdu_hex The PDU hex string.
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
 * @brief Requests the message list from the modem.
 * @return The error code.
 */
std::error_code GsmModem::request_message_list()
{
    std::lock_guard lock(mutex_);
    return write_string("AT+CMGL\r");
}

/**
 * @brief Deletes a message from the modem.
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
 * @brief Classifies a response from the modem.
 * @param data The response data.
 * @return The modem response.
 */
ModemResponse GsmModem::classify_response(std::string_view data)
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

/**
 * @brief Demuxes the URCs from the modem.
 * @param stream The stream to demux.
 * @param out_status_reports The output status reports.
 */
void GsmModem::demux_urcs(std::string& stream, std::vector<Message>& out_status_reports)
{
    for (;;) {
        const std::size_t cds = find_urc_tag(stream, "+CDS:");
        const std::size_t cmt = find_urc_tag(stream, "+CMT:");
        const std::size_t cmti = find_urc_tag(stream, "+CMTI:");
        const std::size_t cdsi = find_urc_tag(stream, "+CDSI:");

        std::size_t begin = std::string::npos;
        enum class Kind { Cds, Cmt, Single } kind = Kind::Single;
        if (cds != std::string::npos) {
            begin = cds;
            kind = Kind::Cds;
        }
        if (cmt != std::string::npos && (begin == std::string::npos || cmt < begin)) {
            begin = cmt;
            kind = Kind::Cmt;
        }
        if (cmti != std::string::npos && (begin == std::string::npos || cmti < begin)) {
            begin = cmti;
            kind = Kind::Single;
        }
        if (cdsi != std::string::npos && (begin == std::string::npos || cdsi < begin)) {
            begin = cdsi;
            kind = Kind::Single;
        }

        if (begin == std::string::npos) {
            return;
        }

        if (kind == Kind::Cds) {
            const int rc = erase_length_urc(stream, begin, "+CDS:", true, &out_status_reports);
            if (rc == 0) {
                return;
            }
            if (rc < 0 && !erase_single_line(stream, begin)) {
                return;
            }
            continue;
        }

        if (kind == Kind::Cmt) {
            const int rc = erase_length_urc(stream, begin, "+CMT:", false, nullptr);
            if (rc == 0) {
                return;
            }
            if (rc < 0 && !erase_single_line(stream, begin)) {
                return;
            }
            continue;
        }

        if (!erase_single_line(stream, begin)) {
            return;
        }
    }
}

/**
 * @brief Demuxes the URCs into the status report queue.
 * @param buffer The buffer to demux.
 */
void GsmModem::demux_into_queue(ResponseBuffer& buffer)
{
    std::vector<Message> reports;
    demux_urcs(buffer.data, reports);
    for (Message& report : reports) {
        status_reports_.push_back(std::move(report));
    }
}

/**
 * @brief Polls the response from the modem.
 * @param buffer The buffer to poll.
 * @return The modem response.
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
 * @brief Polls the response from the modem.
 * @param buffer The buffer to poll.
 * @return The modem response.
 */
ModemResponse GsmModem::poll_response(ResponseBuffer& buffer)
{
    std::lock_guard lock(mutex_);
    return poll_response_unlocked(buffer);
}

/**
 * @brief Waits for the response from the modem.
 * @param buffer The buffer to poll.
 * @param timeout The timeout duration.
 * @param poll_interval The poll interval.
 * @return The modem response.
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
 * @brief Waits for the response from the modem.
 * @param buffer The buffer to poll.
 * @param timeout The timeout duration.
 * @param poll_interval The poll interval.
 * @return The modem response.
 */
ModemResponse GsmModem::wait_for_response(ResponseBuffer& buffer,
                                         std::chrono::milliseconds timeout,
                                         std::chrono::milliseconds poll_interval)
{
    std::lock_guard lock(mutex_);
    return wait_for_response_unlocked(buffer, timeout, poll_interval);
}

/**
 * @brief Waits for the response from the modem.
 * @param buffer The buffer to poll.
 * @param timeout The timeout duration.
 * @param poll_interval The poll interval.
 * @return The modem response.
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
 * @brief Waits for the response from the modem.
 * @param buffer The buffer to poll.
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

/**
 * @brief Polls the status report from the modem.
 * @param out The output status report.
 * @param timeout The timeout duration.
 * @return The error code.
 */
std::error_code GsmModem::poll_status_report(Message& out,
                                             std::chrono::milliseconds timeout)
{
    std::lock_guard lock(mutex_);

    if (!port_->is_open()) {
        return make_error_code(Errc::NotOpen);
    }

    auto try_pop = [&]() -> bool {
        if (status_reports_.empty()) {
            return false;
        }
        out = std::move(status_reports_.front());
        status_reports_.pop_front();
        return true;
    };

    if (try_pop()) {
        return {};
    }

    ResponseBuffer drain;
    (void)poll_response_unlocked(drain);
    if (try_pop()) {
        return {};
    }

    if (timeout.count() <= 0) {
        return make_error_code(Errc::Timeout);
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        drain.data.clear();
        (void)poll_response_unlocked(drain);
        if (try_pop()) {
            return {};
        }
    }

    return make_error_code(Errc::Timeout);
}

/**
 * @brief Gets the number of pending status reports.
 * @return The number of pending status reports.
 */
std::size_t GsmModem::pending_status_reports() const
{
    std::lock_guard lock(mutex_);
    return status_reports_.size();
}

/**
 * @brief Parses the message list from the modem.
 * @param buffer The buffer to parse.
 * @return The messages.
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

} // namespace comettextel
