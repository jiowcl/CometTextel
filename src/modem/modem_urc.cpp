/**
 * @file modem_urc.cpp
 * @brief Modem URC demux, CNMI setup, and status-report queue.
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#include "comettextel/modem.hpp"
#include "comettextel/pdu.hpp"

#include "modem/detail/profile.hpp"
#include "modem/detail/urc.hpp"

#include <cctype>
#include <chrono>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace comettextel {
namespace {

/**
 * @brief Checks if a string is at the start of a line.
 * @param text The string to check.
 * @param index The index to check.
 * @return True if the string is at the start of a line, false otherwise.
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
 * @brief Finds the first occurrence of a URC tag in a string.
 * @param text The string to search.
 * @param tag The URC tag to find.
 * @return The index of the first occurrence of the URC tag, or std::string_view::npos if not found.
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
 * @param text The string to skip spaces in.
 * @param index The index to skip spaces from.
 * @return The index after the spaces.
 */
[[nodiscard]] std::size_t skip_spaces(std::string_view text, std::size_t index)
{
    while (index < text.size() && (text[index] == ' ' || text[index] == '\t')) {
        ++index;
    }
    return index;
}

/**
 * @brief Erases a range of characters from a string.
 * @param stream The string to erase from.
 * @param begin The index to erase from.
 * @param end The index to erase to.
 */
void erase_through(std::string& stream, std::size_t begin, std::size_t end)
{
    if (begin > end || end > stream.size()) {
        return;
    }
    stream.erase(begin, end - begin);
}

/**
 * @brief Erases a single line from a string.
 * @param stream The string to erase from.
 * @param begin The index to erase from.
 * @return True if the line was erased, false otherwise.
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
 * @brief Erases a URC with a length from a string.
 * @param stream The string to erase from.
 * @param begin The index to erase from.
 * @param tag The URC tag to erase.
 * @param decode_status Whether to decode the status report.
 * @param out_status_reports The vector to store the status reports in.
 * @return The error code.
 */
[[nodiscard]] int erase_length_urc(std::string& stream,
                                   std::size_t begin,
                                   std::string_view tag,
                                   bool decode_status,
                                   std::vector<Message>* out_status_reports)
{
    std::size_t cursor = skip_spaces(stream, begin + tag.size());
    if (cursor >= stream.size() || !std::isdigit(static_cast<unsigned char>(stream[cursor]))) {
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

namespace detail::modem::urc {

/**
 * @brief Demuxes URCs from a string.
 * @param stream The string to demux.
 * @param out_status_reports The vector to store the status reports in.
 */
void demux(std::string& stream, std::vector<Message>& out_status_reports)
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

} // namespace detail::modem::urc

/**
 * @brief Demuxes URCs from a string.
 * @param stream The string to demux.
 * @param out_status_reports The vector to store the status reports in.
 */
void GsmModem::demux_urcs(std::string& stream, std::vector<Message>& out_status_reports)
{
    detail::modem::urc::demux(stream, out_status_reports);
}

/**
 * @brief Demuxes URCs from a string into a queue.
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
 * @brief Configures the status report URC.
 */
void GsmModem::configure_status_report_urc()
{
    const auto& profile = detail::modem::profile_for(resolved_profile_id_);

    for (std::size_t i = 0; i < profile.cnmi_command_count; ++i) {
        ResponseBuffer buffer;
        const char* cmd = profile.cnmi_commands[i];
        if (write_string(cmd)) {
            continue;
        }
        buffer.data.clear();
        if (!wait_until_ok_unlocked(buffer, std::chrono::seconds(2), std::chrono::milliseconds(50))) {
            return;
        }
    }
}

/**
 * @brief Polls a status report.
 * @param out The message to store the status report in.
 * @param timeout The timeout duration.
 * @return The error code.
 */
std::error_code GsmModem::poll_status_report(Message& out, std::chrono::milliseconds timeout)
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

} // namespace comettextel
