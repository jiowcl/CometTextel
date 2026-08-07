/**
 * @file send_example.cpp
 * @brief Minimal example that submits one UCS-2 SMS through a GSM modem.
 *
 * Usage:
 *   comettextel_send_example <port> <smsc> <destination> <message>
 *   comettextel_send_example COM3 886932000000 886912345678 "Hello"
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @email jiowcl@gmail.com
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#include <iostream>
#include <string>

#include "comettextel/modem.hpp"

int main(int argc, char* argv[])
{
    if (argc < 5) {
        std::cerr << "Usage: " << argv[0]
                  << " <port> <smsc> <destination> <message>\n";
        return 1;
    }

    comettextel::GsmModem modem;
    comettextel::SerialConfig config;
    config.baud_rate = 115200;

    if (const auto ec = modem.open_and_init(argv[1], config); ec) {
        std::cerr << "open/init failed: " << ec.message() << '\n';
        return 2;
    }

    comettextel::Message message;
    message.service_center = argv[2];
    message.peer_address = argv[3];
    message.coding = comettextel::DataCoding::Ucs2;
    message.user_data = argv[4];

    std::size_t written = 0;
    
    if (const auto ec = modem.send_message(message, &written); ec) {
        std::cerr << "send failed: " << ec.message() << '\n';
        return 3;
    }

    std::cout << "PDU payload written: " << written << " bytes\n";

    return 0;
}
