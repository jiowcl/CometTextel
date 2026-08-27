"""C ABI compatibility tests for legacy native libraries."""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

_SDK_ROOT = Path(__file__).resolve().parents[1]
if str(_SDK_ROOT) not in sys.path:
    sys.path.insert(0, str(_SDK_ROOT))

from comettextel import (  # noqa: E402
    CometTextelError,
    Status,
    decode_status_report,
)
from comettextel import _lib  # noqa: E402
from comettextel.pdu import status_string  # noqa: E402


_REQUIRED_EXPORTS = (
    "ct_status_string",
    "ct_modem_create",
    "ct_modem_destroy",
    "ct_modem_open",
    "ct_modem_send",
    "ct_modem_send_ex",
    "ct_modem_list",
    "ct_modem_delete",
    "ct_pdu_encode_submit",
    "ct_pdu_encode_submit_ex",
    "ct_pdu_encode_submit_segments",
    "ct_pdu_encode_submit_segments_ex",
    "ct_pdu_decode",
)


class _FakeFunction:
    def __init__(self, result: object = 0) -> None:
        self.result = result

    def __call__(self, *args: object) -> object:
        return self.result


class _FakeLibrary:
    def __init__(self, *, noncallable: set[str] | None = None) -> None:
        self._exports = {
            name: _FakeFunction(b"success" if name == "ct_status_string" else 0)
            for name in _REQUIRED_EXPORTS
        }
        self._noncallable = noncallable or set()

    def __getattr__(self, name: str) -> object:
        if name in self._noncallable:
            return object()
        if name in self._exports:
            return self._exports[name]
        raise AttributeError(name)


def _load_fake(
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
    fake: _FakeLibrary,
) -> None:
    monkeypatch.setattr(_lib, "_lib", None)
    monkeypatch.setattr(
        _lib,
        "find_library",
        lambda path=None: tmp_path / "legacy-comettextel.dll",
    )
    monkeypatch.setattr(_lib.ctypes, "WinDLL", lambda path: fake, raising=False)
    monkeypatch.setattr(_lib.ctypes, "CDLL", lambda path: fake, raising=False)
    monkeypatch.setattr(_lib.os, "add_dll_directory", lambda path: None, raising=False)


def test_legacy_dll_keeps_existing_api_usable(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    _load_fake(monkeypatch, tmp_path, _FakeLibrary())

    loaded = _lib.load()

    assert loaded is not None
    assert _lib.api_version() == 1
    assert status_string(Status.OK) == "success"

    with pytest.raises(CometTextelError) as error:
        decode_status_report("00")
    assert error.value.status == Status.UNSUPPORTED
    assert "version 1" in str(error.value)


def test_noncallable_optional_exports_are_ignored(
    monkeypatch: pytest.MonkeyPatch,
    tmp_path: Path,
) -> None:
    fake = _FakeLibrary(
        noncallable={"ct_api_version", "ct_pdu_decode_status_report"}
    )
    _load_fake(monkeypatch, tmp_path, fake)

    assert _lib.load() is fake
    assert _lib.api_version() == 1

    with pytest.raises(CometTextelError) as error:
        decode_status_report("00")
    assert error.value.status == Status.UNSUPPORTED
