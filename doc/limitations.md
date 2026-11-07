# Limitations  

- **Send auto-splits** long text with concatenated SMS UDH (IEI `0x00`, ≤ 255 parts). `PduCodec::encode` remains single-segment and still rejects over-limit payloads.  
- **UDH on receive**: when TP-UDHI is set, the header is **skipped** so `user_data` is payload text only (`Message::has_udh == true`).  
- **Concat**: IEI `0x00` / `0x08` fill `is_concatenated` / `concat_*`. `PduCodec::reassemble_messages` (and `GsmModem::parse_message_list`) join **complete** segment sets into one message (`concat_seq == 0`, `is_reassembled_concat()`). Incomplete sets stay as separate segments.  
- **Per-segment payload caps**:  
  - Single (no UDH): GSM 7-bit ≤ **160** septets; 8-bit / UCS-2 ≤ **140** octets  
  - Concat (with UDH): GSM 7-bit ≤ **153** septets; 8-bit / UCS-2 ≤ **134** octets  
- **GSM 7-bit** maps UTF-8 via the GSM 03.38 default alphabet and ESC extension table (`[]{}\\~^|€`, etc.). Unsupported glyphs return encode failure (use UCS-2). Extension characters consume **two** septets; concat splits never break an ESC pair.  
- **TP-VP / TP-SRR**: submit PDU APIs omit validity period and status-report request by default. Optional relative TP-VP (`0..255`, where `0` is 5 minutes) and TP-SRR are available through the options / C ABI `_ex` APIs.
- **SMS-STATUS-REPORT**: `PduCodec::decode` recognizes MTI=10 and exposes TP-MR, TP-RA, TP-SCTS, TP-DT, and TP-Status through `Message`. Modem helpers demux `+CDS` URCs into a poll queue (`poll_status_report` / `ct_modem_poll_status_report`) after best-effort `AT+CNMI`.
- **Generic AT queries**: `GsmModem::run_at_command` / `ct_modem_run_at_command` (C ABI v3) send one command and wait for OK/ERROR with URC demux during the wait.
