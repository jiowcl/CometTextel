/**
 * @file receive_example.cpp
 * @brief Reads the modem inbox and prints full message bodies.
 *
 * Optionally waits for unsolicited modem traffic (for example @c +CMTI) before
 * listing stored messages with @c AT+CMGL.
 *
 * Usage:
 *   comettextel_receive_example <port> [wait_seconds]
 *   comettextel_receive_example COM3
 *   comettextel_receive_example COM3 15
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @email jiowcl@gmail.com
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

#include "example_common.hpp"

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <port> [wait_seconds]\n";
        return 1;
    }

    const int wait_seconds = (argc >= 3) ? std::atoi(argv[2]) : 0;
    if (wait_seconds < 0) {
        std::cerr << "wait_seconds must be >= 0\n";
        return 1;
    }

    comettextel::GsmModem modem;
    if (const auto ec = modem.open_and_init(argv[1], example::default_config()); ec) {
        std::cerr << "open/init failed: " << ec.message() << '\n';
        return 2;
    }

    comettextel::ResponseBuffer notice;
    if (wait_seconds > 0) {
        std::cout << "Waiting up to " << wait_seconds
                  << "s for modem notifications...\n";
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(wait_seconds);

        while (std::chrono::steady_clock::now() < deadline) {
            const auto status = modem.poll_response(notice);
            if (status == comettextel::ModemResponse::Error) {
                std::cerr << "modem error while waiting:\n" << notice.data << '\n';
                return 3;
            }
            if (notice.data.find("+CMTI") != std::string::npos) {
                std::cout << "Detected +CMTI notification\n";
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    if (const auto ec = modem.request_message_list(); ec) {
        std::cerr << "AT+CMGL failed: " << ec.message() << '\n';
        return 4;
    }

    comettextel::ResponseBuffer buffer;
    if (const auto ec = modem.wait_until_ok(buffer, std::chrono::seconds(10)); ec) {
        std::cerr << "wait for inbox failed: " << ec.message() << '\n';
        std::cerr << buffer.data << '\n';
        return 5;
    }

    const auto messages = comettextel::GsmModem::parse_message_list(buffer);
    std::cout << "Inbox contains " << messages.size() << " message(s)\n";
    for (const auto& message : messages) {
        example::print_message_summary(message, true);
    }

    return 0;
}
