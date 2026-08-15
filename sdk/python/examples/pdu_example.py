#!/usr/bin/env python3
#--------------------------------------------------------------------------------------------
#  CometTextel — Python PDU example (no modem).
#
#  Usage:
#    python pdu_example.py
#    python pdu_example.py <destination> <text> [smsc]
#
#  Place comettextel.dll / libcomettextel.so where the loader can find it, or set
#  COMETTEXTEL_LIB. See ../README.md.
#
#  Copyright (c) Ji-Feng Tsai. All rights reserved.
#  Code released under the MIT license.
#--------------------------------------------------------------------------------------------

from __future__ import annotations

import sys
from pathlib import Path

# Allow running from examples/ without installing the package.
_SDK_ROOT = Path(__file__).resolve().parents[1]
if str(_SDK_ROOT) not in sys.path:
    sys.path.insert(0, str(_SDK_ROOT))

from comettextel import (  # noqa: E402
    CometTextelError,
    DCS_UCS2,
    decode,
    encode_submit,
    encode_submit_segments,
)


def print_usage() -> None:
    """Print usage information."""

    print("Usage:")
    print("  python pdu_example.py")
    print("  python pdu_example.py <destination> <text> [smsc]")


def print_decoded(pdu_hex: str) -> None:
    """Print decoded message information."""
    
    msg = decode(pdu_hex)
    print(
        f"peer={msg.peer_address} text={msg.user_data} dcs={msg.dcs} "
        f"has_udh={int(msg.has_udh)} concat={int(msg.is_concatenated)} "
        f"ref={msg.concat_ref} total={msg.concat_total} seq={msg.concat_seq}"
    )


def run_pdu(destination: str, text: str, smsc: str = "") -> int:
    """Run PDU example."""

    parts = encode_submit_segments(destination, text, smsc, DCS_UCS2)
    for part in parts:
        print(part)
        print_decoded(part)
    print(f"segments={len(parts)}")
    return 0


def run_self_check() -> int:
    """Run self-check."""

    dest = "886912345678"
    smsc = "886932000000"

    print("-- self-check UCS-2 ASCII --")
    run_pdu(dest, "Hello from Python", smsc)

    print("-- self-check UCS-2 Chinese --")
    chinese = "測試中文簡訊"
    parts = encode_submit_segments(dest, chinese, smsc, DCS_UCS2)
    for part in parts:
        print(part)
        msg = decode(part)
        print_decoded(part)
        if msg.user_data != chinese or msg.peer_address != dest:
            print("Chinese UCS-2 round-trip mismatch")
            return 2
    print("ok (6 CJK chars, UCS-2)")
    print(f"segments={len(parts)}")

    print("-- self-check UCS-2 concat (71 ASCII → 2 segments) --")
    long_text = "B" * 71
    parts = encode_submit_segments(dest, long_text, smsc, DCS_UCS2)
    if len(parts) != 2:
        print(f"expected 2 segments, got {len(parts)}")
        return 2
    joined = ""
    for i, part in enumerate(parts):
        print(part)
        msg = decode(part)
        print_decoded(part)
        if not msg.is_concatenated or msg.concat_total != 2 or msg.concat_seq != i + 1:
            print("concat metadata mismatch")
            return 2
        joined += msg.user_data
    if joined != long_text:
        print("concat payload round-trip mismatch")
        return 2
    print("ok")

    print("-- self-check single-segment EncodeSubmit --")
    hex_one = encode_submit(dest, "Hello", smsc, DCS_UCS2)
    print(hex_one)
    msg = decode(hex_one)
    if msg.user_data != "Hello" or msg.peer_address != dest:
        print("round-trip mismatch")
        return 2
    print("ok")
    return 0


def main(argv: list[str]) -> int:
    """Main function."""

    try:
        if len(argv) == 1:
            return run_self_check()
        if len(argv) < 3:
            print_usage()
            return 1
        destination = argv[1]
        text = argv[2]
        smsc = argv[3] if len(argv) >= 4 else ""
        return run_pdu(destination, text, smsc)
    except FileNotFoundError as exc:
        print(exc, file=sys.stderr)
        return 2
    except CometTextelError as exc:
        print(exc, file=sys.stderr)
        return 2


if __name__ == "__main__":
    """Main function."""
    
    raise SystemExit(main(sys.argv))
