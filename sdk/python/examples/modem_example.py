#!/usr/bin/env python3
#--------------------------------------------------------------------------------------------
#  CometTextel — Python modem example (list / send / delete).
#
#  Usage:
#    python modem_example.py list <port> [baud]
#    python modem_example.py send <port> <smsc> <destination> <text> [baud]
#    python modem_example.py delete <port> <index> [baud]
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

_SDK_ROOT = Path(__file__).resolve().parents[1]
if str(_SDK_ROOT) not in sys.path:
    sys.path.insert(0, str(_SDK_ROOT))

from comettextel import CometTextelError, DCS_UCS2, GsmModem  # noqa: E402


def print_usage() -> None:
    print("Usage:")
    print("  python modem_example.py list <port> [baud]")
    print("  python modem_example.py send <port> <smsc> <destination> <text> [baud]")
    print("  python modem_example.py delete <port> <index> [baud]")


def parse_baud(text: str, fallback: int = 115200) -> int:
    try:
        value = int(text)
    except ValueError:
        return fallback
    return value if value > 0 else fallback


def run_list(port: str, baud: int) -> int:
    with GsmModem() as modem:
        modem.open(port, baud)
        messages = modem.list()
        for msg in messages:
            flag = " reassembled" if msg.is_reassembled_concat else ""
            print(
                f"[{msg.index}] {msg.peer_address}: {msg.user_data}"
                f" (dcs={msg.dcs} concat={int(msg.is_concatenated)}"
                f" seq={msg.concat_seq}/{msg.concat_total}{flag})"
            )
        print(f"count={len(messages)}")
    return 0


def run_send(port: str, smsc: str, destination: str, text: str, baud: int) -> int:
    with GsmModem() as modem:
        modem.open(port, baud)
        modem.send(destination, text, smsc=smsc, dcs=DCS_UCS2)
    print("Sent.")
    return 0


def run_delete(port: str, index: int, baud: int) -> int:
    with GsmModem() as modem:
        modem.open(port, baud)
        modem.delete(index)
    print("Deleted.")
    return 0


def main(argv: list[str]) -> int:
    try:
        if len(argv) < 3:
            print_usage()
            return 1
        cmd = argv[1].lower()
        if cmd == "list":
            baud = parse_baud(argv[3]) if len(argv) >= 4 else 115200
            return run_list(argv[2], baud)
        if cmd == "send":
            if len(argv) < 6:
                print_usage()
                return 1
            baud = parse_baud(argv[6]) if len(argv) >= 7 else 115200
            return run_send(argv[2], argv[3], argv[4], argv[5], baud)
        if cmd == "delete":
            if len(argv) < 4:
                print_usage()
                return 1
            baud = parse_baud(argv[4]) if len(argv) >= 5 else 115200
            return run_delete(argv[2], int(argv[3]), baud)
        print_usage()
        return 1
    except FileNotFoundError as exc:
        print(exc, file=sys.stderr)
        return 2
    except CometTextelError as exc:
        print(exc, file=sys.stderr)
        return 2
    except ValueError as exc:
        print(exc, file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
