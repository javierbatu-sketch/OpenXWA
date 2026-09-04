#!/usr/bin/env python3
from pathlib import Path
import sys

if len(sys.argv) != 3:
    raise SystemExit("usage: xwau_extract_flight_resync_impl.py <flight.c> <output.c>")

source = Path(sys.argv[1]).read_text(encoding="utf-8")
start = source.find("int Flight_ComputeWorldStateResyncSegmentSize(int size)")
end_marker = "#pragma pack(push, 1)\ntypedef struct FlightWorldStatePresenceObjectRecord"
end = source.find(end_marker, start)
if start < 0 or end < 0:
    raise SystemExit("could not locate resync helper functions in flight.c")
block = source[start:end]
required = [
    "int Flight_ComputeWorldStateResyncSegmentSize(int size)",
    "int Flight_BuildWorldStateResyncSegmentChecksums(",
]
for token in required:
    if token not in block:
        raise SystemExit(f"missing expected token: {token}")
Path(sys.argv[2]).write_text("#define XWA_MODERN 1\n#include <stdint.h>\n" + block, encoding="utf-8")
