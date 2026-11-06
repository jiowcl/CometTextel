# CometTextel — Go (thin FFI)

Windows **amd64** sample that calls the same C ABI as the C SDK (`c_api.h`).
This is **not** a second native library: it **runtime-loads** `comettextel.dll` with `syscall.LoadDLL`. No cgo and no import library.

Covers **PDU** helpers and **modem** (`Open` / `Send` / `List` / `Delete`, plus C ABI v3 `PollStatusReport` / `RunAtCommand`). Run locally with `verify.ps1` (Go is not required on the CI image).

![Go](https://img.shields.io/badge/language-Go-blue.svg)

## Layout

```text
sdk/go/
├── go.mod
├── lib.go            # LoadDLL + ct_message / ct_status_report
├── pdu.go            # encode / decode
├── modem.go          # modem handle
├── selfcheck.go      # PDU fixtures (no modem)
├── pdu_test.go
├── cmd/pdu/main.go   # encode / decode console sample
├── cmd/modem/main.go # list / send / delete
├── verify.ps1
└── README.md
```

## Prerequisites

| Item | Detail |
|------|--------|
| OS | Windows 10+ x64 |
| Compiler | Go 1.22+ **windows/amd64** (`CGO_ENABLED` may stay 0) |
| Native | C SDK **`comettextel.dll` only** (no `.lib`, no C compiler) |

Download `comettextel-c-sdk-windows-x64` from CI Artifacts or a GitHub Release, or use a local `build/Release/comettextel.dll`.

`ensure()` loads the DLL from, in order:

1. `COMETTEXTEL_LIB` (file or directory)
2. the executable directory
3. the current directory
4. `sdk/go` and common build / artifact folders in this repo

## Test & run

```powershell
cd sdk\go
.\verify.ps1
.\verify.ps1 -GoPath "C:\Program Files\Go\bin\go.exe"
.\verify.ps1 -DllPath ..\..\build\Release\comettextel.dll
```

No arguments on `pdu.exe` runs the PDU self-check:

- UCS-2 ASCII round-trip
- UCS-2 Chinese (`測試中文簡訊`)
- UCS-2 concat (71× `B` → 2 segments with UDH)
- Single-segment `ct_pdu_encode_submit`
- SMS-STATUS-REPORT fixture

Modem (needs a real port):

```bat
modem.exe list COM3
modem.exe send COM3 886932000000 886912345678 "Hello"
modem.exe delete COM3 1
```

`List` rejoins complete concat sets (`ConcatSeq == 0`, `IsReassembledConcat`).

## FFI notes

- `syscall.LoadDLL` / `FindProc`. On Windows amd64 the C ABI calling convention matches `syscall.SyscallN`.
- `EncodeSubmit`, `EncodeSubmitSegments`, and `Modem.Send` take `(smsc, destination, text)`, the same order as `c_api.h`. The `pdu.exe` command line is `<destination> <text> [smsc]`.
- Native text is **UTF-8**. Go `string` values are passed as NUL-terminated bytes.
- `ctMessage` uses `[N]byte` fields sized like `char[N]`.
- `DecodeStatusReport` needs C ABI version 2. `APIVersion` reports version 1 when `ct_api_version` is absent, and that call returns `ErrUnsupported`.
- `PollStatusReport` and `RunAtCommand` need C ABI version 3. `RunAtCommand` copies at most 4095 bytes and a trailing NUL; truncation does not change the status code.
- `EncodeSubmit` is single-segment. `EncodeSubmitSegments` auto-splits with concat UDH.
- `DCSGSM7` uses GSM 03.38 (UTF-8 in/out). Characters outside that alphabet need `DCSUCS2`.
- `EncodeSubmit` / `Send` omit TP-VP. `EncodeSubmitEx` / `SendEx` take a relative TP-VP of 0..255 (`0` = 5 minutes) and a non-zero flag for TP-SRR.

Do not reimplement PDU codecs in Go. If the C ABI changes, update this package only.

## See also

- [`include/comettextel/c_api.h`](https://github.com/jiowcl/CometTextel/blob/main/include/comettextel/c_api.h)
- [`sdk/c/README.md`](https://github.com/jiowcl/CometTextel/blob/main/sdk/c/README.md)
- [`sdk/python/README.md`](https://github.com/jiowcl/CometTextel/blob/main/sdk/python/README.md)
- [`sdk/freebasic/README.md`](https://github.com/jiowcl/CometTextel/blob/main/sdk/freebasic/README.md)
- [`sdk/purebasic/README.md`](https://github.com/jiowcl/CometTextel/blob/main/sdk/purebasic/README.md)
- [`examples/c_api_example.c`](https://github.com/jiowcl/CometTextel/blob/main/examples/c_api_example.c)
