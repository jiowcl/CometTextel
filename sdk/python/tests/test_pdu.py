"""PDU round-trip tests (require comettextel shared library)."""

from __future__ import annotations

import os
import sys
from pathlib import Path

import pytest

_SDK_ROOT = Path(__file__).resolve().parents[1]
if str(_SDK_ROOT) not in sys.path:
    sys.path.insert(0, str(_SDK_ROOT))

from comettextel import (  # noqa: E402
    DCS_GSM7,
    DCS_UCS2,
    CometTextelError,
    decode,
    decode_status_report,
    encode_submit,
    encode_submit_segments,
)
from comettextel import _lib  # noqa: E402


def _have_native() -> bool:
    """Check if the comettextel shared library is found."""

    try:
        _lib.find_library(os.environ.get("COMETTEXTEL_LIB"))
        return True
    except FileNotFoundError:
        return False


pytestmark = pytest.mark.skipif(not _have_native(), reason="comettextel shared library not found")


def test_ascii_roundtrip() -> None:
    """Test ASCII roundtrip."""

    parts = encode_submit_segments("886912345678", "Hello from Python", "886932000000", DCS_UCS2)
    assert len(parts) == 1
    msg = decode(parts[0])
    assert msg.peer_address == "886912345678"
    assert msg.user_data == "Hello from Python"
    assert msg.dcs == DCS_UCS2
    assert not msg.is_concatenated


def test_chinese_roundtrip() -> None:
    """Test Chinese roundtrip."""

    text = "測試中文簡訊"
    parts = encode_submit_segments("886912345678", text, "886932000000", DCS_UCS2)
    assert len(parts) == 1
    # Must not be U+FFFD replacement noise in the PDU.
    assert "FFFD" not in parts[0]
    assert "6E2C" in parts[0].upper()
    msg = decode(parts[0])
    assert msg.user_data == text


def test_ucs2_concat_two_segments() -> None:
    """Test UCS2 concatenated two segments."""

    text = "B" * 71
    parts = encode_submit_segments("886912345678", text, "886932000000", DCS_UCS2)
    assert len(parts) == 2
    joined = ""
    for i, part in enumerate(parts):
        msg = decode(part)
        assert msg.is_concatenated
        assert msg.concat_total == 2
        assert msg.concat_seq == i + 1
        joined += msg.user_data
    assert joined == text


def test_encode_submit_single() -> None:
    """Test encode submit single."""

    hex_pdu = encode_submit("886912345678", "Hello", "886932000000", DCS_UCS2)
    msg = decode(hex_pdu)
    assert msg.user_data == "Hello"


def test_encode_submit_rejects_or_errors_on_empty_dest() -> None:
    """Test encode submit rejects or errors on empty destination."""

    with pytest.raises(CometTextelError):
        encode_submit("", "Hello", "886932000000", DCS_UCS2)


def test_gsm7_ascii_roundtrip() -> None:
    hex_pdu = encode_submit("886912345678", "Hello GSM7", "886932000000", DCS_GSM7)
    msg = decode(hex_pdu)
    assert msg.user_data == "Hello GSM7"
    assert msg.dcs == DCS_GSM7


def test_gsm7_escape_and_at_sign_roundtrip() -> None:
    text = "Cost: 10€ [ok] @home"
    hex_pdu = encode_submit("886912345678", text, "886932000000", DCS_GSM7)
    msg = decode(hex_pdu)
    assert msg.user_data == text
    assert msg.dcs == DCS_GSM7


def test_gsm7_rejects_cjk() -> None:
    with pytest.raises(CometTextelError):
        encode_submit("886912345678", "你好", "886932000000", DCS_GSM7)


def test_submit_defaults_to_no_validity_period() -> None:
    text = "Hi"
    hex_pdu = encode_submit("886912345678", text, "886932000000", DCS_GSM7)
    raw = bytes.fromhex(hex_pdu)
    first_octet_offset = 1 + raw[0]
    assert raw[first_octet_offset] == 0x01


def test_submit_relative_validity_and_status_report() -> None:
    text = "Hi"
    hex_pdu = encode_submit(
        "886912345678",
        text,
        "886932000000",
        DCS_GSM7,
        relative_validity_period=0x00,
        request_status_report=True,
    )
    raw = bytes.fromhex(hex_pdu)
    first_octet_offset = 1 + raw[0]
    assert raw[first_octet_offset] == 0x31
    decoded = decode(hex_pdu)
    assert decoded.user_data == text


def test_gsm7_escape_forces_concat() -> None:
    # 159×A + € => 161 septets → two segments under encode_submit_segments.
    text = ("A" * 159) + "€"
    parts = encode_submit_segments("886912345678", text, "886932000000", DCS_GSM7)
    assert len(parts) == 2
    joined = "".join(decode(p).user_data for p in parts)
    assert joined == text


def test_status_report_decode() -> None:
    pdu = "00022A0C91889621436587215070418045232150704180452300"
    report = decode_status_report(pdu)
    assert report.message_reference == 0x2A
    assert report.tp_status == 0
    assert report.recipient_address == "886912345678"
    assert report.service_timestamp == "12050714085432"
    assert report.discharge_time == "12050714085432"
