"""Locate and load ``comettextel`` shared library; bind PDU exports.

Copyright (c) Ji-Feng Tsai. All rights reserved.
Code released under the MIT license.
"""

from __future__ import annotations

import ctypes
import os
import sys
from ctypes import (
    POINTER,
    c_char,
    c_char_p,
    c_int,
    c_int32,
    c_size_t,
    c_void_p,
)
from pathlib import Path
from typing import Optional

# Layout must match struct ct_message in include/comettextel/c_api.h.
class CtMessage(ctypes.Structure):
    """Structure representing a CT message."""
    _fields_ = [
        ("index", c_int32),
        ("dcs", c_int32),
        ("has_udh", c_int32),
        ("service_center", c_char * 32),
        ("peer_address", c_char * 32),
        ("service_timestamp", c_char * 32),
        ("user_data", c_char * 512),
        ("is_concatenated", c_int32),
        ("concat_ref", c_int32),
        ("concat_total", c_int32),
        ("concat_seq", c_int32),
    ]


_lib: Optional[ctypes.CDLL] = None


def _candidate_names() -> list[str]:
    """Return a list of candidate library names."""
    if sys.platform == "win32":
        return ["comettextel.dll"]
    if sys.platform == "darwin":
        return ["libcomettextel.dylib", "comettextel.dylib"]
    return ["libcomettextel.so", "libcomettextel.so.1", "comettextel.so"]


def _search_dirs(explicit: Optional[Path]) -> list[Path]:
    """Search for the shared library in the given directories."""
    dirs: list[Path] = []
    if explicit is not None:
        dirs.append(explicit if explicit.is_dir() else explicit.parent)

    env = os.environ.get("COMETTEXTEL_LIB")
    if env:
        p = Path(env)
        dirs.append(p if p.is_dir() else p.parent)

    # sdk/python/comettextel/_lib.py → parents[1]=sdk/python, [2]=sdk, [3]=repo root
    here = Path(__file__).resolve()
    sdk_python = here.parents[1]
    repo = here.parents[3] if len(here.parents) > 3 else here.parents[-1]

    dirs.extend(
        [
            Path.cwd(),
            sdk_python,
            sdk_python / "examples",
            repo / "artifact" / "comettextel-c-sdk-windows-x64" / "bin",
            repo / "artifact" / "comettextel-c-sdk-linux-x64" / "lib",
            repo / "build-c-sdk" / "Release",
            repo / "build-c-sdk" / "Debug",
            repo / "build" / "Release",
            repo / "build" / "Debug",
        ]
    )

    # Deduplicate while preserving order.
    seen: set[str] = set()
    out: list[Path] = []
    for d in dirs:
        key = str(d.resolve()) if d.exists() else str(d)
        if key not in seen:
            seen.add(key)
            out.append(d)
    return out


def find_library(path: Optional[str | Path] = None) -> Path:
    """Resolve the shared library path.

    Search order: explicit path / ``COMETTEXTEL_LIB`` / cwd / common build &
    artifact folders next to this package.
    """

    explicit: Optional[Path] = Path(path) if path else None
    if explicit is not None and explicit.is_file():
        return explicit.resolve()

    names = _candidate_names()
    for directory in _search_dirs(explicit):
        if not directory.is_dir():
            continue
        for name in names:
            candidate = directory / name
            if candidate.is_file():
                return candidate.resolve()

    raise FileNotFoundError(
        "comettextel shared library not found. Set COMETTEXTEL_LIB to the DLL/SO "
        "path (or its directory), or place it next to the example / on PATH. "
        f"Looked for: {', '.join(names)}"
    )


def load(path: Optional[str | Path] = None) -> ctypes.CDLL:
    """Load (or return cached) shared library and configure PDU prototypes."""

    global _lib
    if _lib is not None and path is None:
        return _lib

    lib_path = find_library(path)
    if sys.platform == "win32":
        # Ensure dependent MSVC runtime resolution from the DLL folder.
        os.add_dll_directory(str(lib_path.parent))
        loaded = ctypes.WinDLL(str(lib_path))
    else:
        loaded = ctypes.CDLL(str(lib_path))

    loaded.ct_status_string.argtypes = [c_int]
    loaded.ct_status_string.restype = c_char_p

    loaded.ct_pdu_encode_submit.argtypes = [
        c_char_p,
        c_char_p,
        c_char_p,
        c_int,
        c_void_p,
        c_size_t,
    ]
    loaded.ct_pdu_encode_submit.restype = c_int

    loaded.ct_pdu_encode_submit_segments.argtypes = [
        c_char_p,
        c_char_p,
        c_char_p,
        c_int,
        c_void_p,
        c_size_t,
        POINTER(c_int),
    ]
    loaded.ct_pdu_encode_submit_segments.restype = c_int

    loaded.ct_pdu_decode.argtypes = [c_char_p, POINTER(CtMessage)]
    loaded.ct_pdu_decode.restype = c_int

    _lib = loaded
    return _lib


def reset() -> None:
    """Drop the cached CDLL (mainly for tests)."""
    
    global _lib
    _lib = None
