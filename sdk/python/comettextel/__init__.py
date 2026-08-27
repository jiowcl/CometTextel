"""CometTextel — thin Python FFI for the C ABI (PDU + modem).

Requires the shared library from the C SDK (`comettextel.dll` / `libcomettextel.so`).
Does not reimplement PDU codecs.

Copyright (c) Ji-Feng Tsai. All rights reserved.
Code released under the MIT license.
"""

from __future__ import annotations

from .errors import CometTextelError, Status
from . import _lib
from .modem import GsmModem
from .pdu import (
    DCS_8BIT,
    DCS_GSM7,
    DCS_UCS2,
    Message,
    StatusReport,
    decode,
    decode_status_report,
    encode_submit,
    encode_submit_segments,
    status_string,
)

__all__ = [
    "CometTextelError",
    "Status",
    "DCS_GSM7",
    "DCS_8BIT",
    "DCS_UCS2",
    "Message",
    "StatusReport",
    "GsmModem",
    "decode",
    "decode_status_report",
    "encode_submit",
    "encode_submit_segments",
    "status_string",
    "api_version",
]

__version__ = "1.5.0"

api_version = _lib.api_version
