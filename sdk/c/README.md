# CometTextel C SDK

Prebuilt package for consuming CometTextel from **C** (or any language that can link a C ABI) via `comettextel/c_api.h`.

This is the same stable ABI used by **CometTextel.NET** (P/Invoke). Prefer the **shared** library for C apps.

## Layout

**Windows (`comettextel-c-sdk-windows-x64`)**

```text
├── bin/comettextel.dll
├── lib/comettextel.lib              # import library
├── lib/comettextel_static.lib       # optional static (C++ runtime required)
├── include/comettextel/c_api.h
├── examples/c_api_example.c
├── LICENSE
└── README.md                        # this file
```

**Linux (`comettextel-c-sdk-linux-x64`)**

```text
├── lib/libcomettextel.so*
├── lib/libcomettextel.a             # optional static
├── include/comettextel/c_api.h
├── examples/c_api_example.c
├── LICENSE
└── README.md
```

Let `<SDK>` be the extracted package root.

## Quick start (PDU only — no modem)

### Windows (MSVC, shared)

```bat
cl /nologo /W4 /utf-8 /I<SDK>\include ^
  <SDK>\examples\c_api_example.c ^
  /Fe:c_api_example.exe ^
  /link /LIBPATH:<SDK>\lib comettextel.lib

copy <SDK>\bin\comettextel.dll .
c_api_example.exe pdu 886912345678 "Hello" 886932000000
```

### Linux (shared)

```bash
cc -std=c11 -Wall -Wextra \
  -I<SDK>/include \
  <SDK>/examples/c_api_example.c \
  -L<SDK>/lib -lcomettextel -Wl,-rpath,<SDK>/lib \
  -o c_api_example

./c_api_example pdu 886912345678 "Hello" 886932000000
```

## Modem commands (optional)

```text
c_api_example list <port> [baud]
c_api_example send <port> <smsc> <destination> <text> [baud]
c_api_example delete <port> <index> [baud]
```

Examples: Windows `COM3`, Linux `/dev/ttyUSB0`. The modem must speak AT commands in **PDU mode**.

## API surface

| Area | Functions |
|------|-----------|
| Status | `ct_status_string` |
| Modem | `ct_modem_create`, `ct_modem_destroy`, `ct_modem_open`, `ct_modem_send`, `ct_modem_list`, `ct_modem_delete` |
| PDU | `ct_pdu_encode_submit`, `ct_pdu_encode_submit_segments`, `ct_pdu_decode` |

All text fields are **UTF-8**. See `include/comettextel/c_api.h` for details.

## Static linking note

Static archives are produced by a **C++** toolchain. Linking them from a pure C executable typically requires a C++ linker (e.g. `clang++` / `g++`) and the C++ runtime. Prefer the shared library unless you know you need static.

When using the static library, define `COMETTEXTEL_STATIC` before including `c_api.h`.

## Limitations

- `ct_pdu_encode_submit` is single-segment; `ct_pdu_encode_submit_segments` and `ct_modem_send` auto-split with concat UDH (IEI 0x00).
- On receive, UDH is skipped (`has_udh`); concat IEI 0x00 / 0x08 fill `is_concatenated` / `concat_*`. Parts are not joined.

## See also

- Full C++ package / `find_package`: repository [README](https://github.com/jiowcl/CometTextel) and `doc/getting-started.md`
- .NET: `nuget/CometTextel.NET`
