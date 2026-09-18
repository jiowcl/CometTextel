/**
 * @file modem_integration_test.cpp
 * @brief Modem integration tests using in-memory scripted serial.
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @email jiowcl@gmail.com
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#include <chrono>
#include <iostream>
#include <string>
#include <string_view>

#include "comettextel/modem.hpp"
#include "comettextel/types.hpp"

#ifdef COMETTEXTEL_BUILD_C_API
#include "comettextel/c_api.h"
#endif

namespace {

int g_failures = 0;

void expect(bool condition, std::string_view expression, std::string_view file, int line)
{
    if (!condition) {
        std::cerr << file << ':' << line << ": CHECK failed: " << expression << '\n';
        ++g_failures;
    }
}

#define CHECK(expr) ::expect(static_cast<bool>(expr), #expr, __FILE__, __LINE__)

constexpr char kStatusReportPdu[] =
    "00022A0C91889621436587215070418045232150704180452300";

[[nodiscard]] bool is_basic_at(std::string_view cmd)
{
    return cmd == "AT\r" || cmd == "ATE0\r" || cmd == "AT+CMGF=0\r";
}

[[nodiscard]] std::string ok_response()
{
    return "\r\nOK\r\n";
}

[[nodiscard]] std::string error_response()
{
    return "\r\nERROR\r\n";
}

void test_initialize_succeeds_when_cnmi_rejected()
{
    comettextel::SerialPort port;
    CHECK(!port.open_memory([](std::string_view cmd) -> std::string {
        if (is_basic_at(cmd)) {
            return ok_response();
        }
        if (cmd.rfind("AT+CNMI=", 0) == 0) {
            return error_response();
        }
        return ok_response();
    }));

    comettextel::GsmModem modem(port);
    CHECK(!modem.initialize());
    CHECK(modem.port().is_open());
}

void test_poll_status_report_timeout_when_empty()
{
    comettextel::SerialPort port;
    CHECK(!port.open_memory([](std::string_view cmd) -> std::string {
        if (is_basic_at(cmd) || cmd.rfind("AT+CNMI=", 0) == 0) {
            return ok_response();
        }
        return ok_response();
    }));

    comettextel::GsmModem modem(port);
    CHECK(!modem.initialize());
    CHECK(modem.pending_status_reports() == 0);

    comettextel::Message report;
    const auto ec = modem.poll_status_report(report, std::chrono::milliseconds(0));
    CHECK(ec == comettextel::make_error_code(comettextel::Errc::Timeout));
    CHECK(modem.pending_status_reports() == 0);
}

void test_cds_interleaved_with_ok_is_queued()
{
    comettextel::SerialPort port;
    bool awaiting_pdu = false;

    CHECK(!port.open_memory([&](std::string_view cmd) -> std::string {
        if (is_basic_at(cmd) || cmd.rfind("AT+CNMI=", 0) == 0) {
            return ok_response();
        }
        if (cmd.rfind("AT+CMGS=", 0) == 0) {
            awaiting_pdu = true;
            return "\r\n> ";
        }
        if (awaiting_pdu) {
            awaiting_pdu = false;
            // URC arrives before the final OK for the submit.
            return std::string("\r\n+CDS: 25\r\n") + kStatusReportPdu + "\r\n" +
                   ok_response();
        }
        return ok_response();
    }));

    comettextel::GsmModem modem(port);
    CHECK(!modem.initialize());

    comettextel::Message submit;
    submit.peer_address = "886912345678";
    submit.service_center = "886932000000";
    submit.user_data = "Hi";
    submit.coding = comettextel::DataCoding::Gsm7Bit;
    submit.request_status_report = true;

    CHECK(!modem.send_message(submit));
    CHECK(modem.pending_status_reports() >= 1);

    comettextel::Message report;
    CHECK(!modem.poll_status_report(report, std::chrono::milliseconds(0)));
    CHECK(report.is_status_report);
    CHECK(report.message_reference == 0x2A);
    CHECK(report.tp_status == 0x00);
    CHECK(report.peer_address == "886912345678");
}

void test_cds_interleaved_with_prompt_still_sends()
{
    comettextel::SerialPort port;
    bool awaiting_pdu = false;

    CHECK(!port.open_memory([&](std::string_view cmd) -> std::string {
        if (is_basic_at(cmd) || cmd.rfind("AT+CNMI=", 0) == 0) {
            return ok_response();
        }
        if (cmd.rfind("AT+CMGS=", 0) == 0) {
            awaiting_pdu = true;
            // Status report URC arrives while waiting for the '>' prompt.
            return std::string("\r\n+CDS: 25\r\n") + kStatusReportPdu + "\r\n> ";
        }
        if (awaiting_pdu) {
            awaiting_pdu = false;
            return ok_response();
        }
        return ok_response();
    }));

    comettextel::GsmModem modem(port);
    CHECK(!modem.initialize());

    comettextel::Message submit;
    submit.peer_address = "886912345678";
    submit.service_center = "886932000000";
    submit.user_data = "Hi";
    submit.coding = comettextel::DataCoding::Gsm7Bit;

    CHECK(!modem.send_message(submit));

    comettextel::Message report;
    CHECK(!modem.poll_status_report(report, std::chrono::milliseconds(0)));
    CHECK(report.is_status_report);
    CHECK(report.message_reference == 0x2A);
}

void test_run_at_command_csq()
{
    comettextel::SerialPort port;
    CHECK(!port.open_memory([](std::string_view cmd) -> std::string {
        if (cmd.rfind("AT+CSQ", 0) == 0) {
            return std::string("\r\n+CSQ: 18,99\r\n") + ok_response();
        }
        if (cmd.rfind("AT+CPIN?", 0) == 0) {
            return std::string("\r\n+CPIN: READY\r\n") + ok_response();
        }
        return ok_response();
    }));

    comettextel::GsmModem modem(port);
    comettextel::ResponseBuffer response;
    CHECK(!modem.run_at_command("AT+CSQ", response, std::chrono::seconds(3)));
    CHECK(response.data.find("+CSQ: 18,99") != std::string::npos);
    CHECK(response.data.find("OK") != std::string::npos);
}

#ifdef COMETTEXTEL_BUILD_C_API
void test_c_api_run_at_command_not_open()
{
    ct_modem* modem = ct_modem_create();
    char buffer[256]{};
    CHECK(ct_modem_run_at_command(modem, "AT+CSQ\r", buffer, sizeof(buffer), 1000) ==
          CT_ERR_NOT_OPEN);
    ct_modem_destroy(modem);
}

void test_c_api_run_at_command_invalid_args()
{
    ct_modem* modem = ct_modem_create();
    char buffer[256]{};
    CHECK(ct_modem_run_at_command(nullptr, "AT\r", buffer, sizeof(buffer), 1000) ==
          CT_ERR_INVALID_ARGUMENT);
    CHECK(ct_modem_run_at_command(modem, nullptr, buffer, sizeof(buffer), 1000) ==
          CT_ERR_INVALID_ARGUMENT);
    CHECK(ct_modem_run_at_command(modem, "AT\r", nullptr, sizeof(buffer), 1000) ==
          CT_ERR_INVALID_ARGUMENT);
    CHECK(ct_modem_run_at_command(modem, "AT\r", buffer, 0, 1000) == CT_ERR_INVALID_ARGUMENT);
    ct_modem_destroy(modem);
}
#endif

void test_push_rx_urc_without_command()
{
    comettextel::SerialPort port;
    CHECK(!port.open_memory([](std::string_view cmd) -> std::string {
        if (is_basic_at(cmd) || cmd.rfind("AT+CNMI=", 0) == 0) {
            return ok_response();
        }
        return ok_response();
    }));

    comettextel::GsmModem modem(port);
    CHECK(!modem.initialize());

    port.memory_push_rx(std::string("\r\n+CDS: 25\r\n") + kStatusReportPdu + "\r\n");

    comettextel::Message report;
    CHECK(!modem.poll_status_report(report, std::chrono::milliseconds(100)));
    CHECK(report.is_status_report);
    CHECK(report.tp_status == 0x00);
}

} // namespace

int main()
{
    test_initialize_succeeds_when_cnmi_rejected();
    test_poll_status_report_timeout_when_empty();
    test_cds_interleaved_with_ok_is_queued();
    test_cds_interleaved_with_prompt_still_sends();
    test_run_at_command_csq();
#ifdef COMETTEXTEL_BUILD_C_API
    test_c_api_run_at_command_not_open();
    test_c_api_run_at_command_invalid_args();
#endif
    test_push_rx_urc_without_command();

    if (g_failures != 0) {
        std::cerr << g_failures << " check(s) failed\n";
        return 1;
    }

    std::cout << "modem integration tests passed\n";
    return 0;
}
