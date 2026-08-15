"""Modem API smoke tests (no hardware required for create/open-fail paths)."""

from __future__ import annotations

import os
import sys
from pathlib import Path

import pytest

_SDK_ROOT = Path(__file__).resolve().parents[1]
if str(_SDK_ROOT) not in sys.path:
    sys.path.insert(0, str(_SDK_ROOT))

from comettextel import CometTextelError, GsmModem, Message  # noqa: E402
from comettextel import _lib  # noqa: E402


def _have_native() -> bool:
    try:
        _lib.find_library(os.environ.get("COMETTEXTEL_LIB"))
        return True
    except FileNotFoundError:
        return False


pytestmark = pytest.mark.skipif(not _have_native(), reason="comettextel shared library not found")


def test_modem_create_close() -> None:
    modem = GsmModem()
    modem.close()
    modem.close()  # idempotent


def test_modem_context_manager() -> None:
    with GsmModem() as modem:
        assert modem is not None


def test_modem_open_invalid_port_raises() -> None:
    with GsmModem() as modem:
        with pytest.raises(CometTextelError):
            modem.open("COM_INVALID_PORT_FOR_COMETTEXTEL_TEST", 115200)


def test_modem_rejects_empty_port() -> None:
    with GsmModem() as modem:
        with pytest.raises(ValueError):
            modem.open("", 115200)


def test_message_reassembled_property() -> None:
    msg = Message(
        peer_address="886912345678",
        user_data="abc",
        dcs=8,
        is_concatenated=True,
        concat_total=2,
        concat_seq=0,
    )
    assert msg.is_reassembled_concat
    part = Message(
        peer_address="886912345678",
        user_data="a",
        dcs=8,
        is_concatenated=True,
        concat_total=2,
        concat_seq=1,
    )
    assert not part.is_reassembled_concat
