#!/usr/bin/env python3
"""Stage or clean platform-native shared libs under comettextel/_native/.

Used before ``python -m build --wheel`` so the wheel embeds the C ABI library.
"""

from __future__ import annotations

import argparse
import platform
import shutil
import sys
from pathlib import Path

SDK_PYTHON = Path(__file__).resolve().parents[1]
NATIVE_ROOT = SDK_PYTHON / "comettextel" / "_native"


def platform_tag() -> str:
    """Get the platform tag for the current platform."""

    system = sys.platform
    machine = platform.machine().lower()
    if system == "win32":
        if machine in ("amd64", "x86_64"):
            return "win_amd64"
        raise SystemExit(f"unsupported Windows arch: {machine}")
    if system.startswith("linux"):
        if machine in ("x86_64", "amd64"):
            return "linux_x86_64"
        raise SystemExit(f"unsupported Linux arch: {machine}")
    raise SystemExit(f"unsupported platform: {system}")


def find_shared_from_csdk(csdk: Path) -> Path:
    """Find the shared library from the C SDK root."""

    if sys.platform == "win32":
        cand = csdk / "bin" / "comettextel.dll"
        if cand.is_file():
            return cand
        raise SystemExit(f"comettextel.dll not found under {csdk}")

    lib = csdk / "lib"
    so = lib / "libcomettextel.so"
    if so.exists():
        return so.resolve()
    matches = sorted(lib.glob("libcomettextel.so*"))
    files = [p for p in matches if p.is_file() and not p.name.endswith(".a")]
    if not files:
        raise SystemExit(f"libcomettextel.so* not found under {lib}")
    return files[0].resolve()


def write_setup_cfg(tag: str) -> Path:
    """Force a platform wheel tag (avoid py3-none-any when natives are embedded)."""

    plat = {
        "win_amd64": "win_amd64",
        "linux_x86_64": "manylinux_2_34_x86_64",
    }.get(tag)
    if plat is None:
        raise SystemExit(f"no wheel plat-name mapping for tag {tag}")

    cfg = SDK_PYTHON / "setup.cfg"
    cfg.write_text(
        "[bdist_wheel]\n"
        f"plat_name = {plat}\n"
        "python_tag = py3\n",
        encoding="utf-8",
    )
    print(f"wrote {cfg} (plat-name={plat})")
    return cfg


def stage_from(csdk: Path) -> Path:
    """Stage the shared library from the C SDK root."""

    src = find_shared_from_csdk(csdk)
    tag = platform_tag()
    dest_dir = NATIVE_ROOT / tag
    dest_dir.mkdir(parents=True, exist_ok=True)
    if sys.platform == "win32":
        dest = dest_dir / "comettextel.dll"
    else:
        dest = dest_dir / "libcomettextel.so"
    shutil.copy2(src, dest)
    print(f"staged {src} -> {dest}")
    write_setup_cfg(tag)
    return dest


def clean() -> None:
    """Clean the staged platform dirs under _native/."""

    if NATIVE_ROOT.is_dir():
        for child in NATIVE_ROOT.iterdir():
            if child.is_dir() and child.name != "__pycache__":
                shutil.rmtree(child)
                print(f"removed {child}")
    cfg = SDK_PYTHON / "setup.cfg"
    if cfg.is_file():
        cfg.unlink()
        print(f"removed {cfg}")


def main(argv: list[str]) -> int:
    """Main entry point."""

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--from",
        dest="csdk",
        type=Path,
        help="C SDK root (…/comettextel-c-sdk-*) containing bin/ or lib/",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Remove staged platform dirs under _native/",
    )
    args = parser.parse_args(argv)
    if args.clean:
        clean()
        return 0
    if not args.csdk:
        parser.error("provide --from <c-sdk-dir> or --clean")
    stage_from(args.csdk.resolve())
    return 0


if __name__ == "__main__":
    """Entry point."""
    
    raise SystemExit(main(sys.argv[1:]))
