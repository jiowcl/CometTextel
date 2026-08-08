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
    const auto status = example::wait_for_response(modem, buffer);

    if (status == comettextel::ModemResponse::Error) {
        std::cerr << "modem error while listing messages\n";
        std::cerr << buffer.data << '\n';

        return 4;
    }

    if (status == comettextel::ModemResponse::Wait) {
        std::cerr << "timed out waiting for message list\n";
        std::cerr << buffer.data << '\n';

        return 5;
    }

    const auto messages = comettextel::GsmModem::parse_message_list(buffer);
    std::cout << "Found " << messages.size() << " message(s)\n";

    for (const auto& message : messages) {
        example::print_message_summary(message, false);
    }

    return 0;
}
