#!/usr/bin/env python3
"""Guard the existing film initial-world-state envelope and Task 5 size plumbing."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
TEXT = (ROOT / "src/xwa/flight/flight.c").read_text(encoding="utf-8")
failures = []

def need(pattern, message, flags=0):
    if re.search(pattern, TEXT, flags) is None:
        failures.append(message)

record = re.search(r"Flight_SaveWorldState\(\);.*?Film_WriteBytesBuffered\(g_worldStateBuffer, loopIdx\);", TEXT, re.S)
if record is None:
    failures.append("missing film initial world-state recording path")
else:
    body = record.group(0)
    if re.search(r"loopIdx\s*=\s*\(uint32_t\)g_worldStateSize\s*;", body) is None:
        failures.append("film recording must store the complete g_worldStateSize including XWES")
    if re.search(r"Film_WriteBytesBuffered\(&loopIdx, sizeof\(loopIdx\)\);", body) is None:
        failures.append("film recording must retain the existing uint32 world-state size envelope")

play = re.search(r"Film_ReadBytes\(&loopIdx, sizeof\(loopIdx\)\);.*?Flight_RestoreWorldState\(\);", TEXT, re.S)
if play is None:
    failures.append("missing film initial world-state playback path")
else:
    body = play.group(0)
    for pattern, message in [
        (r"Film_ReadBytes\(g_worldStateBuffer, loopIdx\);", "film playback must read exactly the recorded world-state byte count"),
        (r"loopIdx > \(uint32_t\)INT_MAX", "film playback must reject a size that cannot fit restore's int contract"),
        (r"worldStateSize\s*=\s*\(int\)loopIdx\s*;", "film playback must publish recorded size before restore"),
        (r"g_worldStateSize\s*=\s*worldStateSize\s*;", "film playback must keep global serialized size consistent"),
    ]:
        if re.search(pattern, body) is None:
            failures.append(message)

need(r"static void FlightWorldState_RestoreCraftData\(.*?CraftExtended_SeedFromLegacy\(dst\);",
     "classic craft restore must seed the sidecar from the restored retail image", re.S)
need(r"void Flight_RestoreWorldState\(void\).*?CraftExtended_ResetAll\(\);.*?extendedSize\s*=.*?worldStateSize.*?CraftExtended_OverlayWorldStateTail",
     "restore must reset all sidecars before classic restore and optional XWES overlay", re.S)

if failures:
    print(f"FAIL: {len(failures)} film/XWES integration violations")
    for failure in failures:
        print(f"- {failure}")
    sys.exit(1)
print("PASS: film initial world-state keeps the existing size+bytes envelope and restores optional XWES")
