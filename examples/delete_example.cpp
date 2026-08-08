/**
 * @file delete_example.cpp
 * @brief Deletes one stored SMS by index (@c AT+CMGD).
 *
 * Usage:
 *   comettextel_delete_example <port> <index>
 *   comettextel_delete_example COM3 1
 *
 * @author Ji-Feng Tsai (Jiowcl)
 * @email jiowcl@gmail.com
 * @copyright Copyright (c) 2026 Jiowcl. All rights reserved.
 */

#include <chrono>
#include <cstdlib>
#include <iostream>

#include "example_common.hpp"

int main(int argc, char* argv[])
{
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <port> <index>\n";
        return 1;
    }

    const int index = std::atoi(argv[2]);
    if (index < 0) {
        std::cerr << "index must be >= 0\n";
        return 1;
    }

    comettextel::GsmModem modem;
    if (const auto ec = modem.open_and_init(argv[1], example::default_config()); ec) {
        std::cerr << "open/init failed: " << ec.message() << '\n';
        return 2;
    }

    if (const auto ec = modem.delete_message(index); ec) {
        std::cerr << "AT+CMGD failed: " << ec.message() << '\n';
        return 3;
    }

    comettextel::ResponseBuffer buffer;
    if (const auto ec = modem.wait_until_ok(buffer, std::chrono::seconds(5)); ec) {
        std::cerr << "delete confirm failed: " << ec.message() << '\n';
        std::cerr << buffer.data << '\n';
        return 4;
    }

    std::cout << "Deleted message at index " << index << '\n';
    return 0;
}
