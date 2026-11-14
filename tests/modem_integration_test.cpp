/**
 * @file modem_integration_test.cpp
 * @brief Modem integration tests using in-memory scripted serial.
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @email jiowcl@gmail.com
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "comettextel/modem.hpp"
#include "comettextel/modem_profile.hpp"
#include "comettextel/types.hpp"

#ifdef COMETTEXTEL_BUILD_C_API
#include "comettextel/c_api.h"
#endif

namespace {

int g_failures = 0;

/**
 * @brief Expects a condition to be true.
 * @param condition The condition to expect.
 * @param expression The expression to expect.
 * @param file The file to expect.
 * @param line The line to expect.
 */
void expect(bool condition, std::string_view expression, std::string_view file, int line)
{
    if (!condition) {
        std::cerr << file << ':' << line << ": CHECK failed: " << expression << '\n';
        ++g_failures;
    }
}

#define CHECK(expr) ::expect(static_cast<bool>(expr), #expr, __FILE__, __LINE__)

/**
 * @brief Status report PDU.
 */
constexpr char kStatusReportPdu[] =
    "00022A0C91889621436587215070418045232150704180452300";

/**
 * @brief Checks if a command is a basic AT command.
 * @param cmd The command to check.
 * @return True if the command is a basic AT command.
 */
[[nodiscard]] bool is_basic_at(std::string_view cmd)
{
    return cmd == "AT\r" || cmd == "ATE0\r" || cmd == "AT+CMGF=0\r";
}

/**
 * @brief OK response.
 * @return The OK response.
 */
[[nodiscard]] std::string ok_response()
{
    return "\r\nOK\r\n";
}

/**
 * @brief ERROR response.
 * @return The ERROR response.
 */
[[nodiscard]] std::string error_response()
{
    return "\r\nERROR\r\n";
}

/**
 * @brief Tests if the modem initializes successfully when CNMI is rejected.
 */
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

/**
 * @brief Tests if the modem polls a status report when there is no status report.
 */
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

/**
 * @brief Tests if the modem queues a status report when a CDS is interleaved with an OK.
 */
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

/**
 * @brief Tests if the modem sends a status report when a CDS is interleaved with a prompt.
 */
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

/**
 * @brief Tests if the modem runs an AT+CSQ command.
 */
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

/**
 * @brief Tests if the modem runs an AT+CSQ command and truncates the response.
 */
void test_run_at_command_response_truncation_contract()
{
    // Mirrors ct_modem_run_at_command: copy min(size, cap-1) then NUL-terminate.
    // C ABI cannot open the in-memory serial backend, so the copy contract is
    // exercised here against a long scripted response.
    comettextel::SerialPort port;
    const std::string long_payload(512, 'X');
    CHECK(!port.open_memory([&](std::string_view cmd) -> std::string {
        if (cmd.rfind("AT+CSQ", 0) == 0) {
            return std::string("\r\n+CSQ: ") + long_payload + "\r\n" + ok_response();
        }
        return ok_response();
    }));

    comettextel::GsmModem modem(port);
    comettextel::ResponseBuffer response;
    CHECK(!modem.run_at_command("AT+CSQ", response, std::chrono::seconds(3)));
    CHECK(response.data.size() > 64);

    char out[64]{};
    constexpr int out_cap = static_cast<int>(sizeof(out));
    const std::size_t max_copy = static_cast<std::size_t>(out_cap - 1);
    const std::size_t copy_len = std::min(response.data.size(), max_copy);
    std::memcpy(out, response.data.data(), copy_len);
    out[copy_len] = '\0';

    CHECK(copy_len == max_copy);
    CHECK(out[out_cap - 1] == '\0');
    CHECK(std::string_view(out).size() == max_copy);
    CHECK(response.data.compare(0, max_copy, out) == 0);
}

#ifdef COMETTEXTEL_BUILD_C_API
/**
 * @brief Tests if the C API modem profile roundtrip works.
 */
void test_c_api_modem_profile_roundtrip()
{
    comettextel::SerialPort port;
    CHECK(!port.open_memory([](std::string_view cmd) -> std::string {
        if (is_basic_at(cmd) || cmd.rfind("AT+CNMI=", 0) == 0) {
            return ok_response();
        }
        return ok_response();
    }));

    ct_modem* modem = ct_modem_create();
    CHECK(modem != nullptr);

    int requested = -1;
    CHECK(ct_modem_set_profile(modem, CT_MODEM_PROFILE_QUECTEL) == CT_OK);
    CHECK(ct_modem_get_profile(modem, &requested) == CT_OK);
    CHECK(requested == CT_MODEM_PROFILE_QUECTEL);

    CHECK(ct_modem_set_profile(modem, 99) == CT_ERR_INVALID_ARGUMENT);

    ct_modem_destroy(modem);
}

/**
 * @brief Tests if the C API runs an AT+CSQ command and returns an error when the modem is not open.
 */
void test_c_api_run_at_command_not_open()
{
    ct_modem* modem = ct_modem_create();
    char buffer[256]{};
    CHECK(ct_modem_run_at_command(modem, "AT+CSQ\r", buffer, sizeof(buffer), 1000) ==
          CT_ERR_NOT_OPEN);
    ct_modem_destroy(modem);
}

/**
 * @brief Tests if the C API runs an AT+CSQ command and returns an error when the arguments are invalid.
 */
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

/**
 * @brief Tests if the Quectel profile sends the preferred CNMI first.
 */
void test_quectel_profile_sends_preferred_cnmi_first()
{
    std::string first_cnmi;

    comettextel::SerialPort port;
    CHECK(!port.open_memory([&](std::string_view cmd) -> std::string {
        if (is_basic_at(cmd)) {
            return ok_response();
        }
        if (cmd.rfind("AT+CNMI=", 0) == 0) {
            if (first_cnmi.empty()) {
                first_cnmi.assign(cmd);
            }
            return ok_response();
        }
        return ok_response();
    }));

    comettextel::GsmModem modem(port);
    modem.set_modem_profile(comettextel::ModemProfileId::Quectel);
    CHECK(!modem.initialize());
    CHECK(modem.resolved_modem_profile() == comettextel::ModemProfileId::Quectel);
    CHECK(first_cnmi == "AT+CNMI=2,1,0,2,1\r");
}

/**
 * @brief Tests if the auto detect resolves the Quectel profile.
 */
void test_auto_detect_resolves_quectel()
{
    std::string first_cnmi;

    comettextel::SerialPort port;
    CHECK(!port.open_memory([&](std::string_view cmd) -> std::string {
        if (is_basic_at(cmd)) {
            return ok_response();
        }
        if (cmd == "AT+CGMI\r") {
            return "\r\nQuectel\r\n" + ok_response();
        }
        if (cmd == "AT+CGMM\r") {
            return "\r\nEC200U\r\n" + ok_response();
        }
        if (cmd.rfind("AT+CNMI=", 0) == 0) {
            if (first_cnmi.empty()) {
                first_cnmi.assign(cmd);
            }
            return ok_response();
        }
        return ok_response();
    }));

    comettextel::GsmModem modem(port);
    modem.set_modem_profile(comettextel::ModemProfileId::Auto);
    CHECK(!modem.initialize());
    CHECK(modem.resolved_modem_profile() == comettextel::ModemProfileId::Quectel);
    CHECK(modem.modem_manufacturer() == "Quectel");
    CHECK(modem.modem_model() == "EC200U");
    CHECK(first_cnmi == "AT+CNMI=2,1,0,2,1\r");
}

/**
 * @brief Tests if the generic profile skips identity queries.
 */
void test_simcom_profile_configures_cpms_and_cmgl4()
{
    std::vector<std::string> cpms_cmds;
    std::string list_cmd;

    comettextel::SerialPort port;
    CHECK(!port.open_memory([&](std::string_view cmd) -> std::string {
        if (is_basic_at(cmd)) {
            return ok_response();
        }
        if (cmd.rfind("AT+CPMS=", 0) == 0) {
            cpms_cmds.emplace_back(cmd);
            return ok_response();
        }
        if (cmd.rfind("AT+CNMI=", 0) == 0) {
            return ok_response();
        }
        if (cmd.rfind("AT+CMGL", 0) == 0) {
            list_cmd.assign(cmd);
            return ok_response();
        }
        return ok_response();
    }));

    comettextel::GsmModem modem(port);
    modem.set_modem_profile(comettextel::ModemProfileId::Simcom);
    CHECK(!modem.initialize());
    CHECK(cpms_cmds.size() >= 1);
    CHECK(cpms_cmds[0] == "AT+CPMS=\"SM\",\"SM\",\"SM\"\r");

    CHECK(!modem.request_message_list());
    CHECK(list_cmd == "AT+CMGL=4\r");
}

/**
 * @brief Tests if the generic profile skips identity queries.
 */
void test_generic_profile_skips_identity_queries()
{
    bool saw_cgmi = false;
    bool saw_cpms = false;
    std::string list_cmd;

    comettextel::SerialPort port;
    CHECK(!port.open_memory([&](std::string_view cmd) -> std::string {
        if (cmd == "AT+CGMI\r" || cmd == "AT+CGMM\r") {
            saw_cgmi = true;
        }
        if (cmd.rfind("AT+CPMS=", 0) == 0) {
            saw_cpms = true;
        }
        if (cmd.rfind("AT+CMGL", 0) == 0) {
            list_cmd.assign(cmd);
        }
        if (is_basic_at(cmd) || cmd.rfind("AT+CNMI=", 0) == 0) {
            return ok_response();
        }
        return ok_response();
    }));

    comettextel::GsmModem modem(port);
    CHECK(!modem.initialize());
    CHECK(!saw_cgmi);
    CHECK(!saw_cpms);
    CHECK(modem.resolved_modem_profile() == comettextel::ModemProfileId::Generic);
    CHECK(!modem.request_message_list());
    CHECK(list_cmd == "AT+CMGL\r");
}

/**
 * @brief Tests if the modem pushes a RX URC without a command.
 */
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

/**
 * @brief Main function.
 * @return 0 if the tests pass, 1 if the tests fail.
 */
int main()
{
    test_initialize_succeeds_when_cnmi_rejected();
    test_poll_status_report_timeout_when_empty();
    test_cds_interleaved_with_ok_is_queued();
    test_cds_interleaved_with_prompt_still_sends();
    test_run_at_command_csq();
    test_run_at_command_response_truncation_contract();
#ifdef COMETTEXTEL_BUILD_C_API
    test_c_api_modem_profile_roundtrip();
    test_c_api_run_at_command_not_open();
    test_c_api_run_at_command_invalid_args();
#endif
    test_quectel_profile_sends_preferred_cnmi_first();
    test_auto_detect_resolves_quectel();
    test_simcom_profile_configures_cpms_and_cmgl4();
    test_generic_profile_skips_identity_queries();
    test_push_rx_urc_without_command();

    if (g_failures != 0) {
        std::cerr << g_failures << " check(s) failed\n";
        return 1;
    }

    std::cout << "modem integration tests passed\n";
    return 0;
}
