# Internal architecture

Public API lives under `include/comettextel/`. Implementation sources under `src/`
use a private include root (`src/`) so modules can include `pdu/detail/...` and
`modem/detail/...` without exposing those headers.

## PDU (`src/pdu/`)

| Unit | Role |
|------|------|
| `src/pdu.cpp` | Hex utilities, UCS-2, address helpers; `PduCodec` entry points |
| `pdu_gsm7.cpp` | GSM 03.38 7-bit pack/unpack |
| `pdu_tpdu_submit.cpp` | SMS-SUBMIT TPDU encode |
| `pdu_tpdu_decode.cpp` | TPDU decode and concat reassembly |
| `detail/utf8_text.*` | UTF-8 text helpers |
| `detail/tpdu_common.*`, `tpdu_limits.hpp` | Shared TPDU fields and limits |
| `detail/gsm7.hpp` | Shared 7-bit tables/helpers |

## Modem (`src/modem/`)

| Unit | Role |
|------|------|
| `src/modem.cpp` | `GsmModem` construction, `run_at_command`, `port()` |
| `modem_init.cpp` | `open_and_init`, `initialize` (PDU mode + CNMI hook) |
| `modem_at_io.cpp` | Serial read/write, OK/ERROR wait, CMGS `>` prompt |
| `modem_urc.cpp` | URC demux, status-report queue, `poll_status_report` |
| `modem_messages.cpp` | `AT+CMGS` / `AT+CMGL` / `AT+CMGD`, list parsing |
| `modem_profiles.cpp` | Vendor profile table and `CGMI`/`CGMM` detection |
| `detail/at_response.hpp` | Final-result classification |
| `detail/profile.hpp` | Internal `ModemProfile` record |
| `detail/urc.hpp` | URC demux API |
| `detail/cmgl.hpp` | `+CMGL` response parsing |

## Serial

| Unit | Role |
|------|------|
| `src/serial/serial_win32.cpp` | Windows backend |
| `src/serial/serial_posix.cpp` | POSIX backend |

## Stable C ABI

`src/c_api.cpp` wraps the C++ types above; behavior tracks `include/comettextel/c_api.h`.

Refactors in this tree are internal unless `include/` or `c_api.h` change.
