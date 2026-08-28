# CometTextel — FreeBASIC (thin FFI)

Windows **x64** sample that calls the same C ABI as the C SDK and CometTextel.NET (`c_api.h`).  
This is **not** a second native library: it **runtime-loads** `comettextel.dll` via `DyLibLoad` / `DyLibSymbol`.

Covers **PDU** helpers and **modem** (`CtModemOpen` / `Send` / `List` / `Delete`). Not built in CI (FreeBASIC is not on the CI image).

![FreeBASIC](https://img.shields.io/badge/language-FreeBASIC-blue.svg)

## Layout

```text
sdk/freebasic/
├── comettextel.bi    # cdecl prototypes + DyLibLoad + PDU + modem helpers
├── pdu_example.bas   # encode / decode console sample + self-check
├── modem_example.bas # list / send / delete
└── README.md
```

## Prerequisites

| Item | Detail |
|------|--------|
| OS | Windows 10+ x64 |
| Compiler | FreeBASIC 1.10+ **x64** (`fbc -x …`) |
| Native | C SDK **`comettextel.dll` only** (no `.lib`) |

Download `comettextel-c-sdk-windows-x64` from CI Artifacts or a GitHub Release (`*-c-sdk-windows-x64.zip`).

Copy the DLL next to the source and next to the `.exe` (run):

```text
sdk/freebasic/
├── comettextel.bi
├── pdu_example.bas
└── comettextel.dll      ← C SDK bin/comettextel.dll
```

`CtInit()` loads the DLL from the exe folder, then the current directory, then the bare name. The DLL **must** export the C ABI (`ct_status_string`, `ct_pdu_*`, `ct_modem_*`).

```bat
dumpbin /exports comettextel.dll | findstr ct_
```

## Compile & run

```bat
cd sdk\freebasic
fbc -x Output\pdu_example.exe pdu_example.bas
copy /Y ..\..\build-c-sdk\Release\comettextel.dll Output\
Output\pdu_example.exe
```

Or use the helper (finds `fbc64` / `fbc` + DLL automatically):

```powershell
cd sdk\freebasic
.\verify.ps1
.\verify.ps1 -FbcPath D:\IT\Developer\freebasic\fbc64.exe
.\verify.ps1 -DllPath ..\..\build-c-sdk\Release\comettextel.dll
```

Note: a standalone FreeBASIC tree often has `fbc64.exe` next to `bin\`, not `bin\fbc.exe`.

No arguments on `pdu_example` runs a self-check:

- UCS-2 ASCII round-trip
- UCS-2 Chinese via `WChr` code points → UTF-8 (avoids source-file encoding / `FFFD`)
- UCS-2 concat (71× `B` → 2 segments with UDH)
- Single-segment `ct_pdu_encode_submit`

Modem (needs a real port):

```bat
modem_example.exe list COM3
modem_example.exe send COM3 886932000000 886912345678 "Hello"
modem_example.exe delete COM3 1
```

`list` rejoins complete concat sets (`concat_seq = 0`).

> Not built in CI (FreeBASIC is not on the runner). Run `verify.ps1` locally after installing FreeBASIC **x64**.

## FFI notes

- `DyLibLoad` / `DyLibSymbol` with `Function CDecl` pointers (same idea as PureBasic’s runtime bind).
- Native text is **UTF-8**. Use `CtUtf8FromW()` / `CtEncodeSubmit*W()` for Unicode `WString` input.
- `CtMessage` uses `UByte` arrays sized like `char[N]` — do **not** use `ZString * N` for those fields (size differs).
- `CtDecodeStatusReport` decodes SMS-STATUS-REPORT into `CtStatusReport`
  (`message_reference`, `tp_status`, recipient, service and discharge timestamps).
- `CtGetApiVersion()` reports the native C ABI version; legacy DLLs are treated
  as version 1 and return `CT_ERR_UNSUPPORTED` for status-report decoding.
- `ct_pdu_encode_submit` is single-segment; `ct_pdu_encode_submit_segments` auto-splits with concat UDH.
- `CT_DCS_GSM7` uses GSM 03.38 (UTF-8 in/out, ESC extension); alphabet outliers need `CT_DCS_UCS2`.
- `CtEncodeSubmit*` / `CtModemSend` omit TP-VP by default. Pass `relativeValidityPeriod` 0..255
  to add relative TP-VP (`0` = 5 minutes), and `requestStatusReport = 1` to set TP-SRR.

Do not reimplement PDU codecs in FreeBASIC. If the C ABI changes, update `comettextel.bi` only.

## See also

- [`include/comettextel/c_api.h`](https://github.com/jiowcl/CometTextel/blob/main/include/comettextel/c_api.h)
- [`sdk/c/README.md`](https://github.com/jiowcl/CometTextel/blob/main/sdk/c/README.md)
- [`sdk/purebasic/README.md`](https://github.com/jiowcl/CometTextel/blob/main/sdk/purebasic/README.md)
- [`sdk/python/README.md`](https://github.com/jiowcl/CometTextel/blob/main/sdk/python/README.md)
- [`examples/c_api_example.c`](https://github.com/jiowcl/CometTextel/blob/main/examples/c_api_example.c)
