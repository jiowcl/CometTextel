"""Status codes mirroring ``enum ct_status`` in ``c_api.h``.

Copyright (c) Ji-Feng Tsai. All rights reserved.
Code released under the MIT license.
"""

from __future__ import annotations

from enum import IntEnum


class Status(IntEnum):
    OK = 0
    INVALID_ARGUMENT = 1
    NOT_OPEN = 2
    ALREADY_OPEN = 3
    IO = 4
    TIMEOUT = 5
    MODEM_REJECTED = 6
    ENCODE = 7
    DECODE = 8
    UNSUPPORTED = 9
    UNKNOWN = 100


class CometTextelError(RuntimeError):
    """Raised when a ``ct_*`` call returns a non-OK status."""

    def __init__(self, status: int, what: str, detail: str = "") -> None:
        self.status = int(status)
        self.what = what
        message = f"{what} failed ({self.status})"
        if detail:
            message = f"{message}: {detail}"
        super().__init__(message)
