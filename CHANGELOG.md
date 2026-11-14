# Changelog

## 1.7.2 (Pre)

- Add a Windows amd64 Go sample (`sdk/go`) that runtime-loads `comettextel.dll`
  with `syscall.LoadDLL` (no cgo): PDU self-check plus modem list / send / delete.
- Refactor PDU implementation: move GSM 03.38 logic to
  `src/pdu/pdu_gsm7.cpp` and shared UTF-8 helpers under `src/pdu/detail/`.
- Split SMS-SUBMIT encode and TPDU decode/reassembly into `pdu_tpdu_submit.cpp`
  / `pdu_tpdu_decode.cpp` with shared `tpdu_common` / `tpdu_limits` (no public
  C++ / C ABI change).
- Split modem URC handling into `src/modem/modem_urc.cpp` with
  `modem/detail/urc.hpp` demux helpers (`GsmModem` URC queue / status-report
  polling unchanged at the public API).
- Split serial AT I/O and response wait into `src/modem/modem_at_io.cpp`
  (`modem/detail/at_response.hpp` for OK/ERROR classification).
- Split SMS AT helpers into `src/modem/modem_messages.cpp` (`AT+CMGS` /
  `AT+CMGL` / `AT+CMGD`, `modem/detail/cmgl.hpp` for list parsing).
- Split modem open/initialize into `src/modem/modem_init.cpp`; add
  [doc/architecture.md](doc/architecture.md) for internal module layout.
- Add vendor modem profiles (`ModemProfileId`, `set_modem_profile`, Auto
  detect via `AT+CGMI`/`AT+CGMM`, Simcom/Quectel CNMI tables); see
  [doc/modem-profiles.md](doc/modem-profiles.md).
- C ABI v4: `ct_modem_set_profile`, `ct_modem_get_*_profile`,
  `ct_modem_get_identity`; Python / .NET / Go bindings updated.

## 1.7.1

- Add `GsmModem::run_at_command` and C ABI `ct_modem_run_at_command`
  (feature version 3) for generic AT queries with URC demux during the wait;
  synchronize Python, .NET, PureBasic, and FreeBASIC.
- Document that `ct_modem_run_at_command` truncates the response to
  `out_response_cap - 1` bytes (NUL-terminated) without a separate overflow
  status; modem OK/ERROR is still reported via the return code.
- Stop tracking Python `__pycache__` / `*.pyc` artifacts.
- Add .NET `RunAtCommand` argument smoke tests and C ABI truncation fixture.

## 1.7.0

- Demultiplex modem URCs (`+CDS`, `+CMT`, `+CMTI`, `+CDSI`) out of command
  responses so Status Reports do not corrupt OK / `>` waits.
- Best-effort `AT+CNMI` during modem initialize to route delivery reports.
- Add `GsmModem::poll_status_report` and C ABI `ct_modem_poll_status_report`
  (feature version 3); synchronize Python, .NET, PureBasic, and FreeBASIC.
- Internal recursive mutex serializes overlapping modem calls.
- Add in-memory `SerialPort` scripted backend for modem integration tests.

## 1.6.0

- Complete GSM 03.38 7-bit default and extension alphabet handling, including
  septet-aware concatenated SMS splitting.
- Add explicit relative TP-VP and TP-SRR options for submit PDU and modem APIs.
- Decode SMS-STATUS-REPORT PDUs, including TP-MR, TP-RA, TP-SCTS, TP-DT,
  TP-Status, and optional TP-PI parameters.
- Add `ct_status_report` and `ct_pdu_decode_status_report` to the stable C ABI.
- Add C ABI feature version reporting (`ct_api_version`, version 2).
- Keep legacy native DLLs usable from SDKs and report unsupported optional
  features clearly.
- Synchronize Python, .NET, PureBasic, and FreeBASIC bindings and fixtures.
