#!/usr/bin/env python3
"""Guard that XWES stays on the existing checksum/chunk/ACK resync protocol."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
FLIGHT = (ROOT / "src/xwa/flight/flight.c").read_text(encoding="utf-8")
NET = (ROOT / "src/xwa/flight/flight_net.c").read_text(encoding="utf-8")
SYNC = (ROOT / "src/xwa/flight/flight_sync.c").read_text(encoding="utf-8")

failures = []

def need(text, pattern, message, flags=0):
    if re.search(pattern, text, flags) is None:
        failures.append(message)

need(FLIGHT, r"int Flight_ComputeWorldStateResyncSegmentSize\(int size\) \{ return size / 124; \}",
     "resync must retain the existing 124-based segment size contract")
need(FLIGHT, r"segmentCount\s*=\s*125;.*?return 125;", "checksum builder must retain 125 segments", re.S)
need(NET, r"segmentCount\s*=\s*Flight_BuildWorldStateResyncSegmentChecksums\([^;]*worldStateSize\);",
     "resync sender must checksum the complete serialized worldStateSize", re.S)
need(NET, r"segmentSize\s*=\s*Flight_ComputeWorldStateResyncSegmentSize\(worldStateSize\);",
     "resync sender must derive chunks from the complete serialized size")
need(NET, r"packetFreeBytes\s*=\s*492;", "resync sender must retain the existing 500-byte packet payload envelope")
need(NET, r"if \(chunkSlot == 16\) \{.*?FlightNet_WaitForWorldStateChunkAcks\(directPlayId, 16\).*?chunkSlot = 0;.*?memset\(g_flightNetWorldStateChunkAcked, 0, sizeof\(g_flightNetWorldStateChunkAcked\)\);",
     "resync sender must continue after each full 16-chunk ACK window", re.S)
need(NET, r"if \(packetFreeBytes < 496\) \{.*?FlightNet_WaitForWorldStateChunkAcks\(directPlayId, chunkSlot \+ 1\)",
     "resync sender must ACK the final partial window", re.S)
need(SYNC, r"FlightNet_SendWorldStateResyncApplyRequest\(senderDpid, worldStateSize\)",
     "resync apply request must carry the full serialized world-state size")

need(SYNC, r"void FlightSync_ReplayResyncMessages\(unsigned int savedWorldStateSize, int serverTickTime\).*?worldStateSize\s*=\s*\(int\)savedWorldStateSize\s*;.*?memcpy\(g_worldStateBuffer, g_worldStateDupBuffer, \(size_t\)savedWorldStateSize\);.*?g_worldStateSize\s*=\s*worldStateSize\s*;",
     "resync receiver must publish the full saved serialized size before restored world messages run", re.S)
need(SYNC, r"Flight_RestoreWorldState\(\);",
     "resync world-message replay must use the normal world-state restore path (classic seed then optional XWES)")

if failures:
    print(f"FAIL: {len(failures)} resync integration violations")
    for failure in failures:
        print(f"- {failure}")
    sys.exit(1)
print("PASS: extended world state remains on the existing 125-segment / 16-chunk ACK resync path")
