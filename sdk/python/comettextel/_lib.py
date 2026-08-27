"""Locate and load ``comettextel`` shared library; bind C ABI exports.

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
    c_uint32,
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


def _is_usable_lib(path: Path) -> bool:
    """True when @p path is a loadable shared library (file or symlink to one)."""
    
    try:
        return path.is_file() and not path.name.endswith(".a")
    except OSError:
        return False


def _glob_libs_in(directory: Path) -> list[Path]:
    """Find versioned shared libs that fixed candidate names may miss."""

    if sys.platform == "win32":
        patterns = ["comettextel.dll"]
    elif sys.platform == "darwin":
        patterns = ["libcomettextel*.dylib", "comettextel*.dylib"]
    else:
        patterns = ["libcomettextel.so*"]

    found: list[Path] = []
    for pattern in patterns:
        for candidate in sorted(directory.glob(pattern)):
            if _is_usable_lib(candidate):
                found.append(candidate)
    return found


def _packaged_native_dir() -> Optional[Path]:
    """Directory of wheel-embedded native libs (``_native/<tag>/``), if present."""

    here = Path(__file__).resolve().parent
    native = here / "_native"
    if sys.platform == "win32":
        tag = "win_amd64"
    elif sys.platform.startswith("linux"):
        tag = "linux_x86_64"
    else:
        return None
    candidate = native / tag
    return candidate if candidate.is_dir() else None


def _search_dirs(explicit: Optional[Path]) -> list[Path]:
    """Search for the shared library in the given directories."""

    dirs: list[Path] = []
    if explicit is not None:
        dirs.append(explicit if explicit.is_dir() else explicit.parent)

    env = os.environ.get("COMETTEXTEL_LIB")
    if env:
        p = Path(env)
        dirs.append(p if p.is_dir() else p.parent)

    packaged = _packaged_native_dir()
    if packaged is not None:
        dirs.append(packaged)

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
            repo / "build",
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

    Search order: explicit path / ``COMETTEXTEL_LIB`` (file or directory) /
    cwd / common build & artifact folders next to this package.
    """

    explicit: Optional[Path] = Path(path) if path else None
    if explicit is not None and _is_usable_lib(explicit):
        return explicit.resolve()

    # Prefer COMETTEXTEL_LIB when it already points at a shared library file
    # (e.g. libcomettextel.so.1.3.0 from the staged Linux C SDK).
    if explicit is None:
        env = os.environ.get("COMETTEXTEL_LIB")
        if env:
            env_path = Path(env)
            if _is_usable_lib(env_path):
                return env_path.resolve()

    names = _candidate_names()
    for directory in _search_dirs(explicit):
        if not directory.is_dir():
            continue
        for name in names:
            candidate = directory / name
            if _is_usable_lib(candidate):
                return candidate.resolve()
        # Fall back to versioned sonames (libcomettextel.so.1.3.0, …).
        versioned = _glob_libs_in(directory)
        if versioned:
            return versioned[0].resolve()

    raise FileNotFoundError(
        "comettextel shared library not found. Set COMETTEXTEL_LIB to the DLL/SO "
        "path (or its directory), or place it next to the example / on PATH. "
        f"Looked for: {', '.join(names)}"
    )


def load(path: Optional[str | Path] = None) -> ctypes.CDLL:
    """Load (or return cached) shared library and configure C ABI prototypes."""

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

    loaded.ct_modem_create.argtypes = []
    loaded.ct_modem_create.restype = c_void_p

    loaded.ct_modem_destroy.argtypes = [c_void_p]
    loaded.ct_modem_destroy.restype = None

    loaded.ct_modem_open.argtypes = [c_void_p, c_char_p, c_uint32]
    loaded.ct_modem_open.restype = c_int

    loaded.ct_modem_send.argtypes = [
        c_void_p,
        c_char_p,
        c_char_p,
        c_char_p,
        c_int,
        c_int,
    ]
    loaded.ct_modem_send.restype = c_int

    loaded.ct_modem_send_ex.argtypes = [
        c_void_p,
        c_char_p,
        c_char_p,
        c_char_p,
        c_int,
        c_int,
        c_int,
        c_int,
    ]
    loaded.ct_modem_send_ex.restype = c_int

    loaded.ct_modem_list.argtypes = [
        c_void_p,
        POINTER(CtMessage),
        c_int,
        POINTER(c_int),
        c_int,
    ]
    loaded.ct_modem_list.restype = c_int

    loaded.ct_modem_delete.argtypes = [c_void_p, c_int, c_int]
    loaded.ct_modem_delete.restype = c_int

    loaded.ct_pdu_encode_submit.argtypes = [
        c_char_p,
        c_char_p,
        c_char_p,
        c_int,
        c_void_p,
        c_size_t,
    ]
    loaded.ct_pdu_encode_submit.restype = c_int

    loaded.ct_pdu_encode_submit_ex.argtypes = [
        c_char_p,
        c_char_p,
        c_char_p,
        c_int,
        c_int,
        c_int,
        c_void_p,
        c_size_t,
    ]
    loaded.ct_pdu_encode_submit_ex.restype = c_int

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

    loaded.ct_pdu_encode_submit_segments_ex.argtypes = [
        c_char_p,
        c_char_p,
        c_char_p,
        c_int,
        c_int,
        c_int,
        c_void_p,
        c_size_t,
        POINTER(c_int),
    ]
    loaded.ct_pdu_encode_submit_segments_ex.restype = c_int

    loaded.ct_pdu_decode.argtypes = [c_char_p, POINTER(CtMessage)]
    loaded.ct_pdu_decode.restype = c_int

    _lib = loaded
    return _lib


def reset() -> None:
    """Drop the cached CDLL (mainly for tests)."""

    global _lib
    _lib = None
