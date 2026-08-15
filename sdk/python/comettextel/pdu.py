"""PDU encode / decode helpers over ``ct_pdu_*``.

Copyright (c) Ji-Feng Tsai. All rights reserved.
Code released under the MIT license.
"""

from __future__ import annotations

import ctypes
from dataclasses import dataclass
from typing import Optional

from . import _lib
from .errors import CometTextelError, Status

DCS_GSM7 = 0
DCS_8BIT = 4
DCS_UCS2 = 8

_DEFAULT_HEX_CAP = 65536


@dataclass(frozen=True)
class Message:
    """Decoded message fields (UTF-8 text)."""

    peer_address: str
    user_data: str
    dcs: int
    has_udh: bool = False
    is_concatenated: bool = False
    concat_ref: int = 0
    concat_total: int = 0
    concat_seq: int = 0
    service_center: str = ""
    service_timestamp: str = ""
    index: int = -1

    @property
    def is_reassembled_concat(self) -> bool:
        """True when list/reassembly joined a complete multi-part set."""
        return self.is_concatenated and self.concat_seq == 0 and self.concat_total > 0


def _as_utf8(text: str) -> bytes:
    """Convert a Python string to a UTF-8 encoded bytes object."""
    
    return text.encode("utf-8")


def _c_z(buf: ctypes.Array) -> str:
    """Convert a C string buffer to a Python string."""

    raw = bytes(buf)
    nul = raw.find(b"\x00")
    if nul >= 0:
        raw = raw[:nul]
    return raw.decode("utf-8", errors="replace")


def message_from_ct(out: _lib.CtMessage) -> Message:
    """Build a :class:`Message` from a native ``ct_message``."""

    return Message(
        index=int(out.index),
        dcs=int(out.dcs),
        has_udh=bool(out.has_udh),
        service_center=_c_z(out.service_center),
        peer_address=_c_z(out.peer_address),
        service_timestamp=_c_z(out.service_timestamp),
        user_data=_c_z(out.user_data),
        is_concatenated=bool(out.is_concatenated),
        concat_ref=int(out.concat_ref),
        concat_total=int(out.concat_total),
        concat_seq=int(out.concat_seq),
    )


def status_string(status: int) -> str:
    """Convert a status code to a string."""

    lib = _lib.load()
    ptr = lib.ct_status_string(int(status))
    if not ptr:
        return ""
    return ptr.decode("utf-8", errors="replace")


def _check(status: int, what: str) -> None:
    """Check if a status code is OK."""

    if status != Status.OK:
        raise CometTextelError(status, what, status_string(status))


def encode_submit(
    destination: str,
    text: str,
    smsc: str = "",
    dcs: int = DCS_UCS2,
    *,
    out_hex_cap: int = _DEFAULT_HEX_CAP,
) -> str:
    """Encode a single-segment submit PDU (hex). Raises if the text needs split."""

    lib = _lib.load()
    buf = ctypes.create_string_buffer(out_hex_cap)
    status = lib.ct_pdu_encode_submit(
        _as_utf8(smsc),
        _as_utf8(destination),
        _as_utf8(text),
        int(dcs),
        buf,
        out_hex_cap,
    )
    _check(status, "ct_pdu_encode_submit")
    return buf.value.decode("ascii")


def encode_submit_segments(
    destination: str,
    text: str,
    smsc: str = "",
    dcs: int = DCS_UCS2,
    *,
    out_hex_cap: int = _DEFAULT_HEX_CAP,
) -> list[str]:
    """Encode one or more submit PDUs; auto-splits with concat UDH when needed."""

    lib = _lib.load()
    buf = ctypes.create_string_buffer(out_hex_cap)
    count = ctypes.c_int(0)
    status = lib.ct_pdu_encode_submit_segments(
        _as_utf8(smsc),
        _as_utf8(destination),
        _as_utf8(text),
        int(dcs),
        buf,
        out_hex_cap,
        ctypes.byref(count),
    )
    _check(status, "ct_pdu_encode_submit_segments")
    joined = buf.value.decode("ascii")
    parts = [p for p in joined.split("\n") if p]
    if count.value and len(parts) != int(count.value):
        raise CometTextelError(
            Status.ENCODE,
            "ct_pdu_encode_submit_segments",
            f"segment count mismatch (api={count.value}, parsed={len(parts)})",
        )
    return parts


def decode(pdu_hex: str) -> Message:
    """Decode a PDU hex string into a :class:`Message`."""

    lib = _lib.load()
    out = _lib.CtMessage()
    status = lib.ct_pdu_decode(_as_utf8(pdu_hex.strip()), ctypes.byref(out))
    _check(status, "ct_pdu_decode")
    return message_from_ct(out)


def load_library(path: Optional[str] = None) -> None:
    """Eagerly load the native library (optional; otherwise lazy on first call)."""

    _lib.load(path)
