#!/usr/bin/env python3
"""Merge this project's Boot and App binaries for programming at address 0."""
import argparse
import hashlib
from pathlib import Path
import struct

APP_BASE = 0x8000
APP_END = 0x7A000


def check_image(data, base, limit):
    if not 8 <= len(data) <= limit - base:
        raise ValueError(f"Image at {base:#x} has invalid size: {len(data)}")
    sp, reset = struct.unpack_from("<II", data)
    if sp % 8 or not (0x1FFF8000 < sp <= 0x20027000 or 0x200F0000 < sp <= 0x200F1000):
        raise ValueError(f"Invalid initial stack pointer: {sp:#x}")
    if not reset & 1 or not base <= (reset & ~1) < base + len(data):
        raise ValueError(f"Reset vector {reset:#x} is outside this image")


def merge(boot, app):
    check_image(boot, 0, APP_BASE)
    check_image(app, APP_BASE, APP_END)
    if len(boot) < 0x420:
        raise ValueError("Boot image does not contain the complete ICG region")
    return boot + b"\xff" * (APP_BASE - len(boot)) + app


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("boot", type=Path)
    parser.add_argument("app", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        data = merge(args.boot.read_bytes(), args.app.read_bytes())
        # Refuse to overwrite an input or an existing production artifact.
        with args.output.open("xb") as output:
            output.write(data)
    except (OSError, ValueError) as error:
        parser.exit(1, f"Error: {error}\n")
    print(f"{args.output}: {len(data)} bytes; program at 0x00000000")
    print(f"SHA256 {hashlib.sha256(data).hexdigest()}")


if __name__ == "__main__":
    main()
