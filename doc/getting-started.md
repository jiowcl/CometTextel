# Getting Started  

Short guide to use a prebuilt **CometTextel** package in your own project.  

## 1. Download a package  

**Option A — GitHub Actions Artifact (every CI run)**  

1. Open [Actions](https://github.com/jiowcl/CometTextel/actions).  
2. Select the latest successful **CI** workflow on `main`.  
3. Download one of:  
   - `comettextel-windows-x64` / `comettextel-linux-x64` (full C++ package)  
   - `comettextel-c-sdk-windows-x64` / `comettextel-c-sdk-linux-x64` (**C ABI only**)  
4. Unzip / extract locally (Artifacts are retained for 14 days).  

**Option B — GitHub Release (versioned)**  

1. Open [Releases](https://github.com/jiowcl/CometTextel/releases).  
2. Download:  
   - `comettextel-<version>-windows-x64.zip`  
   - `comettextel-<version>-linux-x64.tar.gz`  
   - `comettextel-<version>-c-sdk-windows-x64.zip`  
   - `comettextel-<version>-c-sdk-linux-x64.tar.gz`  

## 2. Package layout  

**Windows**  

```text
comettextel-windows-x64/
├── bin/comettextel.dll
├── lib/comettextel.lib          # import library (shared)
├── lib/comettextel_static.lib   # static library
├── include/comettextel/         # public headers
├── LICENSE
└── README.md
```

**Linux**  

```text
comettextel-linux-x64/
├── lib/libcomettextel.so*
├── lib/libcomettextel.a
├── include/comettextel/
├── LICENSE
└── README.md
```

Let `<CT>` be the extracted package root.  

## 3. Include path  

All public headers live under `include/`:  

```cpp
#include "comettextel/comettextel.hpp"
// or individual headers:
// #include "comettextel/modem.hpp"
// #include "comettextel/pdu.hpp"
// #include "comettextel/serial.hpp"
```

Compiler flag:  

```text
-I<CT>/include
```

MSVC: **C/C++ → Additional Include Directories** → `<CT>\include`  

## 4. Link the library  

### Windows (shared DLL)  

```text
Library path:  <CT>/lib
Link with:     comettextel.lib
Runtime:       copy <CT>/bin/comettextel.dll next to your .exe
```

MSVC example:  

- **Additional Library Directories**: `<CT>\lib`  
- **Additional Dependencies**: `comettextel.lib`  
- Put `comettextel.dll` beside the executable (or on `PATH`).  

### Windows (static)  

```text
Link with:  comettextel_static.lib
Define:     COMETTEXTEL_STATIC
```

No DLL copy is required when linking statically.  

### Linux (shared)  

```bash
g++ -std=c++23 app.cpp -I<CT>/include -L<CT>/lib -lcomettextel -Wl,-rpath,<CT>/lib -o app
```

### Linux (static)  

```bash
g++ -std=c++23 app.cpp -I<CT>/include -L<CT>/lib -lcomettextel -DCOMETTEXTEL_STATIC -o app
```

## 5. CMake consumer (recommended)  

If you installed from source (`cmake --install ... --prefix <prefix>`), or point `CMAKE_PREFIX_PATH` at a prefix that contains `lib/cmake/comettextel/`:  

```cmake
cmake_minimum_required(VERSION 3.28)
project(my_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

find_package(comettextel 1.0 REQUIRED CONFIG)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE comettextel::comettextel)
# comettextel::shared  or  comettextel::static  also work
```

Configure:  

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=<prefix>
cmake --build build --config Release
```

**Using only the CI zip (no install tree):** set include/link directories manually, for example:  

```cmake
add_library(comettextel SHARED IMPORTED)
set_target_properties(comettextel PROPERTIES
    IMPORTED_LOCATION             "${CT}/bin/comettextel.dll"
    IMPORTED_IMPLIB               "${CT}/lib/comettextel.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${CT}/include"
)
target_link_libraries(my_app PRIVATE comettextel)
```

On Linux, set `IMPORTED_LOCATION` to `libcomettextel.so` under `${CT}/lib`.  

## 6. Minimal usage  

```cpp
#include <iostream>
#include "comettextel/comettextel.hpp"

int main()
{
    comettextel::GsmModem modem;
    comettextel::SerialConfig config;
    config.baud_rate = 115200;

    // Windows: "COM3"  |  Linux: "/dev/ttyUSB0"
    if (const auto ec = modem.open_and_init("COM3", config); ec) {
        std::cerr << ec.message() << '\n';
        return 1;
    }

    comettextel::Message message;
    message.service_center = "886932000000"; // or empty for modem default SMSC
    message.peer_address = "886912345678";
    message.coding = comettextel::DataCoding::Ucs2;
    message.user_data = "Hello from CometTextel";

    if (const auto ec = modem.send_message(message); ec) {
        std::cerr << ec.message() << '\n';
        return 2;
    }
    return 0;
}
```

Requires a GSM modem that speaks AT commands in **PDU mode**.  

## Limitations  

- **Send auto-splits** long text with concatenated SMS UDH (`PduCodec::encode_segments` / modem send). `PduCodec::encode` remains single-segment.  
- **Receive with UDH**: header octets are skipped (`has_udh`); concat IEI `0x00` / `0x08` fill `is_concatenated` / `concat_*`. Complete sets are joined by `PduCodec::reassemble_messages` / `GsmModem::parse_message_list` (`concat_seq == 0`).  
- Keep single-segment payloads within:  
  - GSM 7-bit ≤ 160 septets after GSM 03.38 mapping (ESC extension chars count as 2)  
  - 8-bit / UCS-2 ≤ 140 bytes  
- Concat per-segment caps: GSM 7-bit ≤ 153 septets; 8-bit / UCS-2 ≤ 134 octets.  
- GSM 7-bit `user_data` is UTF-8; unsupported characters fail encode (prefer UCS-2).  
- Over-limit single `encode` fails with `EncodeFailure` instead of silent truncation.  

## 7. Try the bundled examples  

If you build from source with `COMETTEXTEL_BUILD_EXAMPLES=ON`:  

```bash
comettextel_send_example COM3 886932000000 886912345678 "Hello"
comettextel_list_example COM3
comettextel_receive_example COM3
comettextel_delete_example COM3 1
```

With `COMETTEXTEL_BUILD_C_API=ON` as well:  

```bash
comettextel_c_api_example pdu 886912345678 "Hello" 886932000000
```

## 8. C SDK consumers  

Prefer the dedicated **C SDK** artifact (or Release `*-c-sdk-*` archive). It ships only:

- `include/comettextel/c_api.h`  
- shared (+ optional static) library  
- `examples/c_api_example.c`  
- package `README.md` with compile/link commands  

See [`sdk/c/README.md`](https://github.com/jiowcl/CometTextel/sdk/c/README.md). For a full C++ tree, use the non-`c-sdk` package above.

## See also  

- [README.md](https://github.com/jiowcl/CometTextel/README.md) — build options, CI, releases  
- `examples/` — send / list / receive / delete samples (+ C ABI example)  
- [`sdk/c/README.md`](https://github.com/jiowcl/CometTextel/sdk/c/README.md) — C SDK layout and quick start  
- [`sdk/purebasic/README.md`](https://github.com/jiowcl/CometTextel/sdk/purebasic/README.md) — PureBasic OpenLibrary PDU sample (Windows x64, not in CI)  
- [`sdk/freebasic/README.md`](https://github.com/jiowcl/CometTextel/sdk/freebasic/README.md) — FreeBasic DyLibLoad PDU sample (Windows x64, not in CI)  
