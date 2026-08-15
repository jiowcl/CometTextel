# CometTextel — PureBasic (thin FFI)

Windows **x64** sample that calls the same C ABI as the C SDK and CometTextel.NET (`c_api.h`).  
This is **not** a second native library: it **runtime-loads** `comettextel.dll` via `LoadLibraryW` / `GetProcAddress`.

Save `.pb` / `.pbi` as **UTF-8 with BOM** if you put CJK in the source. Without a BOM, the C backend can turn those literals into `U+FFFD` (PDU hex `FFFD…`). The sample uses `Chr($6E2C)` etc. so it does not depend on file encoding.

PDU only (no modem). Not built in CI (PureBasic is a commercial compiler).

![PureBasic](https://img.shields.io/badge/language-PureBasic-blue.svg)

## Layout

```text
sdk/purebasic/
├── comettextel.pbi   # PrototypeC + OpenLibrary + thin UTF-8 helpers
├── pdu_example.pb    # encode / decode console sample
└── README.md
```

## Prerequisites

| Item | Detail |
|------|--------|
| OS | Windows 10+ x64 |
| Compiler | PureBasic 6.x **x64** (Unicode), C/LLVM backend is fine |
| Native | C SDK **`comettextel.dll` only** (no `.lib`) |

Download `comettextel-c-sdk-windows-x64` from CI Artifacts or a GitHub Release (`*-c-sdk-windows-x64.zip`).

Copy the DLL next to the source (IDE) and next to the `.exe` (run):

```text
sdk/purebasic/
├── comettextel.pbi
├── pdu_example.pb
└── comettextel.dll      ← C SDK bin/comettextel.dll
```

`CtInit()` loads the DLL with `LoadLibraryW` (exe folder, then current directory) and binds `ct_*` via `GetProcAddress`.

The DLL **must** export the C ABI (`ct_status_string`, `ct_pdu_encode_submit`, `ct_pdu_encode_submit_segments`, `ct_pdu_decode`). An older `comettextel.dll` without the C API will load but fail on `GetProcAddress`. Check with:

```bat
dumpbin /exports comettextel.dll | findstr ct_
```

## Compile & run

IDE: open `pdu_example.pb`, target **Windows x64**, compile.

Command line (adjust the PureBasic install path):

```bat
cd sdk\purebasic
pbcompiler pdu_example.pb /EXE pdu_example.exe
pdu_example.exe
pdu_example.exe 886912345678 "Hello" 886932000000
pdu_example.exe 886912345678 "測試中文簡訊" 886932000000
```

No arguments runs a small UCS-2 ASCII + Chinese round-trip self-check.

## FFI notes

- `LoadLibraryW` / `GetProcAddress` (not `ImportC` of the MSVC `.lib`; that fails with PB’s `lld-link`).
- Strings at the native boundary are **UTF-8** (`PokeS(..., #PB_UTF8)` / `PeekS(..., #PB_UTF8)`). Do not pass a stack `UTF8()` pointer into the DLL.
- `CtMessage` must stay packed like `struct ct_message` (`Align #PB_Structure_AlignC`).
- `ct_pdu_encode_submit` is single-segment; `ct_pdu_encode_submit_segments` auto-splits with concat UDH.

Do not reimplement PDU codecs in PureBasic. If the C ABI changes, update `comettextel.pbi` only.

## See also

- [`include/comettextel/c_api.h`](../../include/comettextel/c_api.h)
- [`sdk/c/README.md`](../c/README.md)
- [`sdk/freebasic/README.md`](../freebasic/README.md)
- [`sdk/python/README.md`](../python/README.md)
- [`examples/c_api_example.c`](../../examples/c_api_example.c)
