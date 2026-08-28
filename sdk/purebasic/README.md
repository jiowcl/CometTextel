# CometTextel — PureBasic (thin FFI)

Windows **x64** sample that calls the same C ABI as the C SDK and CometTextel.NET (`c_api.h`).  
This is **not** a second native library: it **runtime-loads** `comettextel.dll` via `LoadLibraryW` / `GetProcAddress`.

Save `.pb` / `.pbi` as **UTF-8 with BOM** if you put CJK in the source. Without a BOM, the C backend can turn those literals into `U+FFFD` (PDU hex `FFFD…`). The sample uses `Chr($6E2C)` etc. so it does not depend on file encoding.

Covers **PDU** helpers and **modem** (`CtModemOpen` / `Send` / `List` / `Delete`). Not built in CI (PureBasic is a commercial compiler).

![PureBasic](https://img.shields.io/badge/language-PureBasic-blue.svg)

## Layout

```text
sdk/purebasic/
├── comettextel.pbi   # PrototypeC + OpenLibrary + PDU + modem helpers
├── pdu_example.pb    # encode / decode console sample
├── modem_example.pb  # list / send / delete
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

The DLL **must** export the C ABI (`ct_status_string`, `ct_pdu_*`, `ct_modem_*`). An older `comettextel.dll` without the C API will load but fail on `GetProcAddress`. Check with:

```bat
dumpbin /exports comettextel.dll | findstr ct_
```

## Compile & run

IDE: open `pdu_example.pb`, target **Windows x64**, compile.

Command line (adjust the PureBasic install path):

```bat
cd sdk\purebasic
pbcompiler pdu_example.pb /EXE Output\pdu_example.exe /CONSOLE
copy /Y ..\..\build-c-sdk\Release\comettextel.dll Output\
Output\pdu_example.exe
```

Or:

```powershell
cd sdk\purebasic
.\verify.ps1
```

```bat
pdu_example.exe 886912345678 "Hello" 886932000000
pdu_example.exe 886912345678 "測試中文簡訊" 886932000000
```

Modem (needs a real port):

```bat
modem_example.exe list COM3
modem_example.exe send COM3 886932000000 886912345678 "Hello"
modem_example.exe delete COM3 1
```

`list` rejoins complete concat sets (`concat_seq = 0`).

No arguments on `pdu_example` runs a self-check:

- UCS-2 ASCII round-trip
- UCS-2 Chinese via `Chr` code points (avoids source-file encoding / `FFFD`)
- UCS-2 concat (71× `B` → 2 segments with UDH)
- Single-segment `ct_pdu_encode_submit`

## FFI notes

- `LoadLibraryW` / `GetProcAddress` (not `ImportC` of the MSVC `.lib`; that fails with PB’s `lld-link`).
- Strings at the native boundary are **UTF-8** (`PokeS(..., #PB_UTF8)` / `PeekS(..., #PB_UTF8)`). Do not pass a stack `UTF8()` pointer into the DLL.
- `CtMessage` must stay packed like `struct ct_message` (`Align #PB_Structure_AlignC`).
- `CtDecodeStatusReport` decodes SMS-STATUS-REPORT into `CtStatusReport`
  (`message_reference`, `tp_status`, recipient, service and discharge timestamps).
- `CtGetApiVersion()` reports the native C ABI version; legacy DLLs are treated
  as version 1 and return `#CT_ERR_UNSUPPORTED` for status-report decoding.
- `ct_pdu_encode_submit` is single-segment; `ct_pdu_encode_submit_segments` auto-splits with concat UDH.
- `#CT_DCS_GSM7` uses GSM 03.38 (UTF-8 in/out, ESC extension); alphabet outliers need `#CT_DCS_UCS2`.
- `CtEncodeSubmit*` / `CtModemSend` omit TP-VP by default. Pass `relativeValidityPeriod` 0..255
  to add relative TP-VP (`0` = 5 minutes), and `requestStatusReport = 1` to set TP-SRR.

Do not reimplement PDU codecs in PureBasic. If the C ABI changes, update `comettextel.pbi` only.

## See also

- [`include/comettextel/c_api.h`](https://github.com/jiowcl/CometTextel/blob/main/include/comettextel/c_api.h)
- [`sdk/c/README.md`](https://github.com/jiowcl/CometTextel/blob/main/sdk/c/README.md)
- [`sdk/freebasic/README.md`](https://github.com/jiowcl/CometTextel/blob/main/sdk/freebasic/README.md)
- [`sdk/python/README.md`](https://github.com/jiowcl/CometTextel/blob/main/sdk/python/README.md)
- [`examples/c_api_example.c`](https://github.com/jiowcl/CometTextel/blob/main/examples/c_api_example.c)
