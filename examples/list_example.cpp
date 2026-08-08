/**
 * @file list_example.cpp
 * @brief Lists SMS messages stored on the modem (@c AT+CMGL).
 *
 * Usage:
 *   comettextel_list_example <port>
 *   comettextel_list_example COM3
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @email jiowcl@gmail.com
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#include <chrono>
#include <iostream>

#include "example_common.hpp"

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <port>\n";
        return 1;
    }

    comettextel::GsmModem modem;
    if (const auto ec = modem.open_and_init(argv[1], example::default_config()); ec) {
        std::cerr << "open/init failed: " << ec.message() << '\n';
        return 2;
    }

    if (const auto ec = modem.request_message_list(); ec) {
        std::cerr << "AT+CMGL failed: " << ec.message() << '\n';
        return 3;
    }

    comettextel::ResponseBuffer buffer;
    if (const auto ec = modem.wait_until_ok(buffer, std::chrono::seconds(8)); ec) {
        std::cerr << "wait for list failed: " << ec.message() << '\n';
        std::cerr << buffer.data << '\n';
        return 4;
    }

    const auto messages = comettextel::GsmModem::parse_message_list(buffer);
    std::cout << "Found " << messages.size() << " message(s)\n";
    for (const auto& message : messages) {
        example::print_message_summary(message, false);
    }

    return 0;
}
