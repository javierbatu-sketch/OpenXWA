#!/usr/bin/env python3
"""Ensure high/XWES bytes can trigger the existing 16-section world mismatch detector."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
text = (ROOT / "src/xwa/flight/flight.c").read_text(encoding="utf-8")

m = re.search(r"int Flight_ChecksumWorldState\(int expectedChecksum, int serverTicks\) \{(.*?)\n\}\n\n// FUNCTION: XWA 0x4F58A0", text, re.S)
if m is None:
    print("FAIL: could not locate Flight_ChecksumWorldState")
    sys.exit(1)
body = m.group(1)
failures = []

if re.search(r"extendedWorldStateBytes\s*=\s*g_worldStateSize\s*-\s*\(int\)\(cursor - g_worldStateBuffer\)", body) is None:
    failures.append("periodic world checksum does not measure bytes after the classic prefix")
if re.search(r"extendedWorldStateChecksum\s*\+=\s*\*extendedCursor\+\+", body) is None:
    failures.append("periodic world checksum does not consume XWES bytes")
if re.search(r"g_worldChecksum\[segmentIndex - 1\]\s*\+=\s*extendedWorldStateChecksum", body) is None:
    failures.append("XWES checksum is not folded into an existing 16-section checksum slot")
if re.search(r"totalChecksum\s*\+=\s*extendedWorldStateChecksum", body) is None:
    failures.append("XWES checksum does not participate in the aggregate mismatch checksum")

if failures:
    print(f"FAIL: {len(failures)} primary checksum/XWES integration violations")
    for failure in failures:
        print(f"- {failure}")
    sys.exit(1)
print("PASS: XWES bytes participate in the existing periodic world mismatch checksum")
