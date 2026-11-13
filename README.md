# CometTextel  

**Tiny, Elegant, and Practical — A Modern C++ Library for GSM SMS over Serial.**  

**CometTextel** is a concise, lightweight C++ library for sending and receiving short messages through a GSM modem. It provides a clean serial abstraction, GSM 03.40 PDU encode/decode, and AT-command helpers — built with **C++23** and **CMake**, ready as a shared or static library.  

![GitHub](https://img.shields.io/badge/license-MIT-green.svg)
![C++](https://img.shields.io/badge/language-C%2B%2B23-blue.svg)
![CMake](https://img.shields.io/badge/build-CMake-brightgreen.svg)
![CI](https://github.com/jiowcl/CometTextel/actions/workflows/ci.yml/badge.svg)
![Status](https://img.shields.io/badge/Status-Active-success)

**First-time integration:** [doc/getting-started.md](doc/getting-started.md) (download Artifact / Release → include & link).  

## Features  

- **Cross-Platform Serial Layer**: Unified `SerialPort` API with Win32 and POSIX backends.  
- **GSM PDU Codec**: 7-bit, 8-bit, and UCS-2 encode/decode without MFC or legacy globals.  
- **Modem Helpers**: Initialize, send (`AT+CMGS`), list (`AT+CMGL`), and delete (`AT+CMGD`) in PDU mode.  
- **Modern C++23**: RAII handles, `std::error_code`, `std::span`, and clear public headers under `include/comettextel/`.  
- **Shared & Static**: Build `comettextel` as a DLL/shared library and/or a static archive from one CMake tree.  
- **Multi-Language & Package Ready:** Pure C-ABI exports with high-level bindings for **C# (.NET / NuGet)**, **Go** (Go Packages), **Python (PyPI)**, **C/C++**, **PureBasic**, and **FreeBASIC**.

## Limitations  

For complete protocol limits, payload capacity tables, and edge-case handling, see [doc/limitations.md](doc/limitations.md).  

Contributor map of `src/pdu/` and `src/modem/` units: [doc/architecture.md](doc/architecture.md).  

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
| `COMETTEXTEL_BUILD_C_API` | `ON` | Build stable C ABI (`c_api.h`) for P/Invoke / NuGet |

Artifacts (Release):  

- Windows: `build/Release/comettextel.dll`, `build/Release/comettextel_static.lib`  
- Unix: `libcomettextel.so` / `libcomettextel.a`  
- Example: `build/examples/Release/comettextel_*_example.exe`  

Run unit tests:  

```bash
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Install & find_package  

```bash
cmake -S . -B build -A x64 -DCOMETTEXTEL_BUILD_EXAMPLES=OFF
cmake --build build --config Release
cmake --install build --config Release --prefix ./prefix
```

Consumer project:  

```cmake
find_package(comettextel 1.0 REQUIRED CONFIG)
target_link_libraries(app PRIVATE comettextel::comettextel)
# or explicitly:
# target_link_libraries(app PRIVATE comettextel::shared)
# target_link_libraries(app PRIVATE comettextel::static)
```

Set `CMAKE_PREFIX_PATH` to the install prefix if needed.  

## API Example  

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

## Examples  

Send one UCS-2 SMS:  

```bash
comettextel_send_example COM3 886932000000 886912345678 "Hello"
```

List stored messages (summary):  

```bash
comettextel_list_example COM3
```

Read the inbox (full text; optional wait for `+CMTI`):  

```bash
comettextel_receive_example COM3
comettextel_receive_example COM3 15
```

Delete one stored message by index:  

```bash
comettextel_delete_example COM3 1
```

## Layout  

```text
comettextel/
├── cmake/                 # Compiler options, Doxygen, package config
├── doc/                   # User guides
│   └── getting-started.md
│   └── limitations.md
│   └── releases.md
│   └── ci.md
├── examples/              # Sample programs (+ c_api_example.c)
├── include/comettextel/   # Public headers (+ c_api.h)
├── nuget/CometTextel.NET/ # Optional .NET NuGet wrapper (P/Invoke; win+linux RID)
├── scripts/               # CI helpers (e.g. stage_c_sdk)
├── sdk/c/                 # C SDK package README template
├── sdk/python/            # Python ctypes PDU + modem (pytest + wheels in CI)
├── sdk/go/                # Go PDU + modem sample (Windows amd64, local verify.ps1)
├── sdk/purebasic/         # PureBasic PDU + modem sample (local verify.ps1)
├── sdk/freebasic/         # FreeBASIC PDU + modem sample (local verify.ps1)
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

## Official Partners & Commercial Editions  

This project collaborate with companies that offer commercial versions, hosted services, or enterprise support based on CometTextel:  

**[KJPro Technology](https://kjprotech.com/en-us/product/textel-pro)** - Official Commercial Partner providing enterprise distribution, custom integration, and dedicated support.  

## TODO  

- Modem async I/O / overlapped serial (beyond URC demux + poll queue)  
- Vendor modem profiles (`CNMI`, inbox quirks)  
- Concat / incomplete-segment edge cases  

## Donation  

If this library helps you reduce time to coding, you can give me a cup of coffee :)  

[![paypal](https://www.paypalobjects.com/en_US/TW/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=3RNMD6Q3B495N&source=url)  

[Paypal Me](https://paypal.me/jiowcl?locale.x=zh_TW)  
