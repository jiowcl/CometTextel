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
- **Doxygen-Ready**: English API comments with author metadata for generated documentation.  

## Limitations  

- **Send auto-splits** long text with concatenated SMS UDH (IEI `0x00`, ≤ 255 parts). `PduCodec::encode` remains single-segment and still rejects over-limit payloads.  
- **UDH on receive**: when TP-UDHI is set, the header is **skipped** so `user_data` is payload text only (`Message::has_udh == true`).  
- **Concat**: IEI `0x00` / `0x08` fill `is_concatenated` / `concat_*`. `PduCodec::reassemble_messages` (and `GsmModem::parse_message_list`) join **complete** segment sets into one message (`concat_seq == 0`, `is_reassembled_concat()`). Incomplete sets stay as separate segments.  
- **Per-segment payload caps**:  
  - Single (no UDH): GSM 7-bit ≤ **160** septets; 8-bit / UCS-2 ≤ **140** octets  
  - Concat (with UDH): GSM 7-bit ≤ **153** septets; 8-bit / UCS-2 ≤ **134** octets  
- GSM 7-bit path treats input bytes as septets; it does not validate the full GSM 03.38 alphabet / escape table.  

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

## Continuous Integration  

GitHub Actions (`.github/workflows/ci.yml`) runs on every push and pull request to `main`:  

- **Windows (MSVC x64)**: configure, build, run CTest, upload `comettextel-windows-x64`  
  (`bin/comettextel.dll`, import/static libs, headers)  
- **Windows C SDK**: upload `comettextel-c-sdk-windows-x64` (`c_api.h` + DLL/libs + `examples/c_api_example.c`)  
- **Windows .NET**: PDU smoke tests (`CometTextel.NET.Tests`, including UCS-2 Chinese), then pack/upload `CometTextel.NET-nupkg`  
- **Linux (GCC 14)**: configure, build, run CTest, upload `comettextel-linux-x64`  
  (`libcomettextel.so*`, `libcomettextel.a`, headers)  
- **Linux C SDK**: upload `comettextel-c-sdk-linux-x64`  

Download build packages from the workflow **Artifacts** tab (retained 14 days).  

For a step-by-step “download → include → link” walkthrough, see [doc/getting-started.md](doc/getting-started.md).  

## C SDK (optional)  

Stable C ABI: [`include/comettextel/c_api.h`](include/comettextel/c_api.h).  
Sample: [`examples/c_api_example.c`](examples/c_api_example.c) (built as `comettextel_c_api_example` when `COMETTEXTEL_BUILD_EXAMPLES=ON` and `COMETTEXTEL_BUILD_C_API=ON`).  

```bash
# After build (Release):
comettextel_c_api_example pdu 886912345678 "Hello" 886932000000
```

CI / Release also publish focused **C SDK** artifacts (`comettextel-c-sdk-*`) with only `c_api.h`, shared/static libs, and the example source — see [`sdk/c/README.md`](sdk/c/README.md).  

Optional language samples (thin FFI, same shared library; not built in CI): [`sdk/python/`](sdk/python) (ctypes, PDU + modem; **pytest runs in CI**), [`sdk/purebasic/`](sdk/purebasic) (PDU; local `verify.ps1`), [`sdk/freebasic/`](sdk/freebasic) (PDU; local `verify.ps1`).  

## .NET NuGet (optional)  

A separate C# wrapper lives under [`nuget/CometTextel.NET`](nuget/CometTextel.NET) (style similar to ThinBasic.NET):  

```powershell
cd nuget/CometTextel.NET
.\pack.ps1
```

This builds `comettextel.dll` with the C ABI and produces `CometTextel.NET.*.nupkg`. See that folder’s README for Install-Package / Quick start.  

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

## Releases  

Push a version tag to publish a GitHub Release with binaries:  

```bash
git tag v1.0.0
git push origin v1.0.0
```

The **Release** workflow (`.github/workflows/release.yml`) builds, tests, and attaches:  

- `comettextel-<version>-windows-x64.zip`  
- `comettextel-<version>-linux-x64.tar.gz`  
- `comettextel-<version>-c-sdk-windows-x64.zip`  
- `comettextel-<version>-c-sdk-linux-x64.tar.gz`  

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
├── examples/              # Sample programs (+ c_api_example.c)
├── include/comettextel/   # Public headers (+ c_api.h)
├── nuget/CometTextel.NET/ # Optional .NET NuGet wrapper (P/Invoke)
├── scripts/               # CI helpers (e.g. stage_c_sdk)
├── sdk/c/                 # C SDK package README template
├── sdk/python/            # Optional Python ctypes PDU + modem sample (not in CI)
├── sdk/purebasic/         # Optional PureBasic PDU sample (not in CI)
├── sdk/freebasic/         # Optional FreeBASIC PDU sample (not in CI)
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

- Longer modem async I/O / thread-safe serial   

## Donation  

If this library helps you reduce time to coding, you can give me a cup of coffee :)  

[![paypal](https://www.paypalobjects.com/en_US/TW/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=3RNMD6Q3B495N&source=url)  

[Paypal Me](https://paypal.me/jiowcl?locale.x=zh_TW)  
