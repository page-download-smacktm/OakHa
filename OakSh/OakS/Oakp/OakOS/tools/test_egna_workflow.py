#!/usr/bin/env python3
import os
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILDER = ROOT / 'tools' / 'egna_builder.py'

if not BUILDER.exists():
    print('missing builder: tools/egna_builder.py')
    sys.exit(1)

with tempfile.TemporaryDirectory() as tmp_dir:
    tmp = Path(tmp_dir)
    icon = tmp / 'icon.bin'
    payload = tmp / 'payload.bin'
    out = tmp / 'hello.egna'

    icon.write_bytes(bytes([0x00, 0xFF, 0x00, 0xFF] * 4)[:16])
    payload.write_bytes(b'EGBC\x01\x02\x03\x04')

    subprocess.run([
        sys.executable,
        str(BUILDER),
        '--name', 'Demo',
        '--shell', 'demo --run',
        '--icon', str(icon),
        '--payload', str(payload),
        '--output', str(out),
    ], check=True)

    data = out.read_bytes()
    if len(data) < 200:
        raise AssertionError('package too short')

    magic, version, flags, name_bytes, shell_bytes, icon_len, icon_w, icon_h, code_len = struct.unpack('<4sII32s128sIIII', data[:4 + 4 + 4 + 32 + 128 + 4 + 4 + 4 + 4])
    if magic != b'EGNA':
        raise AssertionError('bad magic')
    if version != 1:
        raise AssertionError('bad version')
    if icon_len != 16 or icon_w != 4 or icon_h != 4:
        raise AssertionError('bad icon metadata')
    if code_len != len(payload.read_bytes()):
        raise AssertionError('bad payload metadata')

    print('EGNA workflow OK')
