# CometTextel  

**Tiny, Elegant, and Practical — A Modern C++ Library for GSM SMS over Serial.**  

**CometTextel** is a concise, lightweight C++ library for sending and receiving short messages through a GSM modem. It provides a clean serial abstraction, GSM 03.40 PDU encode/decode, and AT-command helpers — built with **C++23** and **CMake**, ready as a shared or static library.  

![GitHub](https://img.shields.io/badge/license-MIT-green.svg)
![C++](https://img.shields.io/badge/language-C%2B%2B23-blue.svg)
![CMake](https://img.shields.io/badge/build-CMake-brightgreen.svg)
![CI](https://github.com/jiowcl/CometTextel/actions/workflows/ci.yml/badge.svg)
![Status](https://img.shields.io/badge/Status-Active-success)

## Features  

- **Cross-Platform Serial Layer**: Unified `SerialPort` API with Win32 and POSIX backends.  
- **GSM PDU Codec**: 7-bit, 8-bit, and UCS-2 encode/decode without MFC or legacy globals.  
- **Modem Helpers**: Initialize, send (`AT+CMGS`), list (`AT+CMGL`), and delete (`AT+CMGD`) in PDU mode.  
- **Modern C++23**: RAII handles, `std::error_code`, `std::span`, and clear public headers under `include/comettextel/`.  
- **Shared & Static**: Build `comettextel` as a DLL/shared library and/or a static archive from one CMake tree.  
- **Doxygen-Ready**: English API comments with author metadata for generated documentation.  

## Environment  

- Windows 10/11, Linux, or macOS  
- CMake 3.28 above (recommend)  
- A C++23 compiler (MSVC 19.4x+, GCC 14+, or Clang 18+)  
- A GSM modem / USB dongle that speaks AT commands in PDU mode  

## How to Build  

Building requires CMake and a C++23 toolchain. Tested under Windows 11 with MSVC.  

```bash
cmake -S . -B build -A x64
cmake --build build --config Release
```

Optional flags:  

| Option | Default | Description |
|--------|---------|-------------|
| `COMETTEXTEL_BUILD_SHARED` | `ON` | Build the shared library |
| `COMETTEXTEL_BUILD_STATIC` | `ON` | Build the static library |
| `COMETTEXTEL_BUILD_EXAMPLES` | `ON` | Build example programs |
| `COMETTEXTEL_BUILD_TESTS` | `ON` | Build unit tests |
| `COMETTEXTEL_BUILD_DOCS` | `OFF` | Generate Doxygen docs |

Artifacts (Release):  

- Windows: `build/Release/comettextel.dll`, `build/Release/comettextel_static.lib`  
- Unix: `libcomettextel.so` / `libcomettextel.a`  
- Example: `build/examples/Release/comettextel_send_example.exe`  

Run unit tests:  

```bash
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Continuous Integration  

GitHub Actions (`.github/workflows/ci.yml`) runs on every push and pull request to `main`:  

- **Windows (MSVC x64)**: configure, build, run CTest, upload `comettextel-windows-x64`  
  (`bin/comettextel.dll`, import/static libs, headers)  
- **Linux (GCC 14)**: configure, build, run CTest, upload `comettextel-linux-x64`  
  (`libcomettextel.so*`, `libcomettextel.a`, headers)  

Download build packages from the workflow **Artifacts** tab (retained 14 days).  

## Example  

```cpp
#include "comettextel/comettextel.hpp"

#include <iostream>

int main()
{
    comettextel::GsmModem modem;
    comettextel::SerialConfig config;
    config.baud_rate = 115200;

    if (const auto ec = modem.open_and_init("COM3", config); ec) {
        std::cerr << "open/init failed: " << ec.message() << '\n';
        return 1;
    }

    comettextel::Message message;
    message.service_center = "886932000000";
    message.peer_address = "886912345678";
    message.coding = comettextel::DataCoding::Ucs2;
    message.user_data = "Hello from CometTextel";

    if (const auto ec = modem.send_message(message); ec) {
        std::cerr << "send failed: " << ec.message() << '\n';
        return 2;
    }

    return 0;
}
```

Or run the bundled example:  

```bash
comettextel_send_example COM3 886932000000 886912345678 "Hello"
```

## Layout  

```text
comettextel/
├── cmake/                 # Compiler options, Doxygen
├── examples/              # Sample programs
├── include/comettextel/   # Public headers
├── src/                   # Library sources
│   └── serial/            # Win32 / POSIX backends
├── tests/                 # Unit tests (CTest)
└── CMakeLists.txt
```

## History  

- 2014: Original Win32 COMM / GSM PDU code written as a Visual C++ project.  
- 2026: Rewritten as a modern C++23 CMake library with cross-platform serial support and renamed to **CometTextel**.  

## Credits  

- Original COM port and PDU routines inspired by classic GSM modem programming samples.  

## License  

Copyright (c) 2026 Ji-Feng Tsai.  
Code released under the MIT license.  

## TODO  

- Longer modem response handling / async I/O  
- More examples (list / delete / receive)  
- Package config (`comettextelConfig.cmake`) for `find_package`  

## Donation  

If this library helps you reduce time to coding, you can give me a cup of coffee :)  

[![paypal](https://www.paypalobjects.com/en_US/TW/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=3RNMD6Q3B495N&source=url)  

[Paypal Me](https://paypal.me/jiowcl?locale.x=zh_TW)  
