#!/usr/bin/env python3
import argparse
import os
import struct
import sys
from pathlib import Path

MAGIC = b'EGNA'
VERSION = 1


def parse_args():
    parser = argparse.ArgumentParser(description='Build an EGNA package')
    parser.add_argument('--name', required=True)
    parser.add_argument('--shell', default='')
    parser.add_argument('--icon', default='')
    parser.add_argument('--payload', default='')
    parser.add_argument('--output', required=True)
    return parser.parse_args()


def pad_name(value):
    return value[:31].encode('utf-8') + b'\0' * (32 - min(len(value), 31))


def pad_text(value, size):
    data = value.encode('utf-8')
    if len(data) >= size:
        return data[:size - 1] + b'\0'
    return data + b'\0' * (size - len(data))


def write_exact(f, data):
    f.write(data)


def main():
    args = parse_args()
    name = args.name
    shell_command = args.shell or ''
    icon_path = Path(args.icon) if args.icon else None
    payload_path = Path(args.payload) if args.payload else None
    output_path = Path(args.output)

    icon_bytes = b''
    if icon_path and icon_path.exists():
        icon_bytes = icon_path.read_bytes()
        if len(icon_bytes) > 65535:
            raise ValueError('icon too large')

    payload_bytes = b''
    if payload_path and payload_path.exists():
        payload_bytes = payload_path.read_bytes()
        if len(payload_bytes) > 16 * 1024 * 1024:
            raise ValueError('payload too large')

    icon_w = 0
    icon_h = 0
    if len(icon_bytes) >= 8:
        # Keep the metadata simple: a 4x4 icon is represented as 16 bytes.
        icon_w = 4
        icon_h = 4

    header = struct.pack(
        '<4sII32s128sIIII',
        MAGIC,
        VERSION,
        0,
        pad_name(name),
        pad_text(shell_command, 128),
        len(icon_bytes),
        icon_w,
        icon_h,
        len(payload_bytes),
    )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, 'wb') as f:
        f.write(header)
        f.write(icon_bytes)
        f.write(payload_bytes)

    print(f'EGNA written to {output_path}')


if __name__ == '__main__':
    try:
        main()
    except Exception as exc:  # pragma: no cover
        print(f'error: {exc}', file=sys.stderr)
        raise
