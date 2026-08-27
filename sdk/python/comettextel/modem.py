"""GSM modem helpers over ``ct_modem_*``.

Copyright (c) Ji-Feng Tsai. All rights reserved.
Code released under the MIT license.
"""

from __future__ import annotations

import ctypes
from types import TracebackType
from typing import Optional, Type

from . import _lib
from .errors import CometTextelError
from .pdu import DCS_UCS2, Message, _as_utf8, _check, message_from_ct


class GsmModem:
    """Context-managed wrapper around an opaque ``ct_modem`` handle."""

    def __init__(self) -> None:
        """Create a new modem object."""
        
        lib = _lib.load()
        handle = lib.ct_modem_create()
        if not handle:
            raise CometTextelError(100, "ct_modem_create", "null handle")
        self._handle = handle
        self._closed = False

    def close(self) -> None:
        """Destroy the native modem (idempotent)."""

        if self._closed:
            return
        lib = _lib.load()
        lib.ct_modem_destroy(self._handle)
        self._handle = None
        self._closed = True

    def __enter__(self) -> GsmModem:
        """Return the modem when the context is entered."""

        return self

    def __exit__(
        self,
        exc_type: Optional[Type[BaseException]],
        exc: Optional[BaseException],
        tb: Optional[TracebackType],
    ) -> None:
        """Close the modem when the context is exited."""

        self.close()

    def __del__(self) -> None:
        """Destroy the modem when the object is garbage collected."""

        try:
            self.close()
        except Exception:
            pass

    def _ensure_open(self) -> ctypes.CDLL:
        """Ensure the modem is open."""

        if self._closed or not self._handle:
            raise CometTextelError(2, "GsmModem", "modem is closed")
        return _lib.load()

    def open(self, port: str, baud_rate: int = 115200) -> None:
        """Open the serial port and initialize PDU mode."""

        if not port:
            raise ValueError("port must be non-empty")
        lib = self._ensure_open()
        status = lib.ct_modem_open(self._handle, _as_utf8(port), int(baud_rate))
        _check(status, "ct_modem_open")

    def send(
        self,
        destination: str,
        text: str,
        smsc: str = "",
        dcs: int = DCS_UCS2,
        timeout_ms: int = 10000,
        *,
        relative_validity_period: Optional[int] = None,
        request_status_report: bool = False,
    ) -> None:
        """Send an SMS with optional relative TP-VP and TP-SRR."""

        if not destination:
            raise ValueError("destination must be non-empty")
        lib = self._ensure_open()
        vp = -1 if relative_validity_period is None else int(relative_validity_period)
        status = lib.ct_modem_send_ex(
            self._handle,
            _as_utf8(smsc),
            _as_utf8(destination),
            _as_utf8(text),
            int(dcs),
            vp,
            int(bool(request_status_report)),
            int(timeout_ms),
        )
        _check(status, "ct_modem_send")

    def list(self, max_count: int = 64, timeout_ms: int = 8000) -> list[Message]:
        """List stored messages (complete concat sets are rejoined)."""

        if max_count <= 0:
            raise ValueError("max_count must be positive")
        lib = self._ensure_open()
        buffer = (_lib.CtMessage * max_count)()
        count = ctypes.c_int(0)
        status = lib.ct_modem_list(
            self._handle,
            buffer,
            int(max_count),
            ctypes.byref(count),
            int(timeout_ms),
        )
        _check(status, "ct_modem_list")
        n = int(count.value)
        return [message_from_ct(buffer[i]) for i in range(n)]

    def delete(self, index: int, timeout_ms: int = 5000) -> None:
        """Delete one stored message by modem storage index."""

        lib = self._ensure_open()
        status = lib.ct_modem_delete(self._handle, int(index), int(timeout_ms))
        _check(status, "ct_modem_delete")
