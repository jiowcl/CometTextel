# Changelog

## Unreleased

- Demultiplex modem URCs (`+CDS`, `+CMT`, `+CMTI`, `+CDSI`) out of command
  responses so Status Reports do not corrupt OK / `>` waits.
- Best-effort `AT+CNMI` during modem initialize to route delivery reports.
- Add `GsmModem::poll_status_report` and C ABI `ct_modem_poll_status_report`
  (feature version 3); synchronize Python, .NET, PureBasic, and FreeBASIC.
- Internal recursive mutex serializes overlapping modem calls.

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
