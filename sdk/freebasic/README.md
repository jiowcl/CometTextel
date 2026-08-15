# CometTextel — FreeBASIC (thin FFI)

Windows **x64** sample that calls the same C ABI as the C SDK and CometTextel.NET (`c_api.h`).  
This is **not** a second native library: it **runtime-loads** `comettextel.dll` via `DyLibLoad` / `DyLibSymbol`.

PDU only (no modem). Not built in CI (FreeBASIC is not on the CI image).

![FreeBASIC](https://img.shields.io/badge/language-FreeBASIC-blue.svg)

## Layout

```text
sdk/freebasic/
├── comettextel.bi    # cdecl prototypes + DyLibLoad + UTF-8 helpers
├── pdu_example.bas   # encode / decode console sample + self-check
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

`CtInit()` loads the DLL from the exe folder, then the current directory, then the bare name. The DLL **must** export the C ABI (`ct_status_string`, `ct_pdu_encode_submit`, `ct_pdu_encode_submit_segments`, `ct_pdu_decode`).

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
No arguments runs a self-check:

- UCS-2 ASCII round-trip
- UCS-2 Chinese via `WChr` code points → UTF-8 (avoids source-file encoding / `FFFD`)
- UCS-2 concat (71× `B` → 2 segments with UDH)
- Single-segment `ct_pdu_encode_submit`

> Not built in CI (FreeBASIC is not on the runner). Run `verify.ps1` locally after installing FreeBASIC **x64**.

## FFI notes

- `DyLibLoad` / `DyLibSymbol` with `Function CDecl` pointers (same idea as PureBasic’s runtime bind).
- Native text is **UTF-8**. Use `CtUtf8FromW()` / `CtEncodeSubmit*W()` for Unicode `WString` input.
- `CtMessage` uses `UByte` arrays sized like `char[N]` — do **not** use `ZString * N` for those fields (size differs).
- `ct_pdu_encode_submit` is single-segment; `ct_pdu_encode_submit_segments` auto-splits with concat UDH.

Do not reimplement PDU codecs in FreeBASIC. If the C ABI changes, update `comettextel.bi` only.

## See also

- [`include/comettextel/c_api.h`](../../include/comettextel/c_api.h)
- [`sdk/c/README.md`](../c/README.md)
- [`sdk/purebasic/README.md`](../purebasic/README.md)
- [`sdk/python/README.md`](../python/README.md)
- [`examples/c_api_example.c`](../../examples/c_api_example.c)
