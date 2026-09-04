#!/usr/bin/env python3
"""Guard the Task 5 Flight/XWES integration contract without changing gameplay semantics."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
FLIGHT = ROOT / "src/xwa/flight/flight.c"


def require(text: str, pattern: str, message: str, flags=0) -> list[str]:
    if re.search(pattern, text, flags) is None:
        return [message]
    return []


def main() -> int:
    text = FLIGHT.read_text(encoding="utf-8")
    failures: list[str] = []

    serialize = re.search(
        r"static void FlightWorldState_SerializeCraftData\(.*?\n\}", text, re.S
    )
    if serialize is None:
        failures.append("missing FlightWorldState_SerializeCraftData")
    else:
        body = serialize.group(0)
        failures += require(body, r"CraftData\s+legacyImage\s*;", "serializer must use a temporary CraftData legacy image")
        failures += require(body, r"CraftExtended_ProjectToLegacy\(src,\s*&legacyImage\s*\)", "serializer must project sidecar into the temporary legacy image")
        failures += require(body, r"memcpy\(dst->bytes,\s*&?legacyImage,\s*offsetof\(CraftData,\s*effectiveAiObjectLink\)\)", "classic craft prefix must be copied from the projected legacy image")

    restore_craft = re.search(
        r"static void FlightWorldState_RestoreCraftData\(.*?\n\}", text, re.S
    )
    if restore_craft is None:
        failures.append("missing FlightWorldState_RestoreCraftData")
    else:
        failures += require(restore_craft.group(0), r"CraftExtended_SeedFromLegacy\(dst\)", "restored classic CraftData must seed authoritative sidecar state")

    save_start = text.find("int Flight_SaveWorldState(void)")
    save_end = text.find("// FUNCTION: XWA 0x4F5C10", save_start)
    if save_start < 0 or save_end < 0:
        failures.append("missing Flight_SaveWorldState")
    else:
        save_body = text[save_start:save_end]
        failures += require(save_body, r"CraftExtended_WriteWorldStateTail\(", "SaveWorldState must append XWES after the classic prefix")
        failures += require(save_body, r"FlightWorldState_WriteBlock\(&cursor,\s*g_players,", "classic player block must remain before XWES")
        if "CraftExtended_WriteWorldStateTail" in save_body and "FlightWorldState_WriteBlock(&cursor, g_players" in save_body:
            if save_body.index("CraftExtended_WriteWorldStateTail") < save_body.index("FlightWorldState_WriteBlock(&cursor, g_players"):
                failures.append("XWES must be appended only after the complete classic prefix")

    restore_start = text.find("void Flight_RestoreWorldState(void)")
    restore_end = text.find("// Craft steering embeds", restore_start)
    if restore_start < 0 or restore_end < 0:
        failures.append("missing Flight_RestoreWorldState")
    else:
        body = text[restore_start:restore_end]
        failures += require(body, r"CraftExtended_ResetAll\(\)", "restore must reset sidecars before classic restore")
        failures += require(body, r"cursor\s*\+=\s*3023u\s*\*\s*\(uint32_t\)g_flightPlayerCount", "restore must advance past classic player bytes before examining XWES")
        failures += require(body, r"CraftExtended_OverlayWorldStateTail\(", "restore must validate/overlay optional XWES")
        failures += require(body, r"worldStateSize", "restore must use the serialized size to distinguish no tail from invalid tail")

    failures += require(text, r"CraftExtended_WorldStateBufferSizeWithTail\(", "world-state allocation must add XWES worst-case capacity with checked size_t arithmetic")

    alloc = re.search(r"void Flight_AllocWorldStateBuffers\(void\).*?// FUNCTION: XWA 0x4F5280", text, re.S)
    if alloc is None:
        failures.append("missing Flight_AllocWorldStateBuffers")
    else:
        failures += require(
            alloc.group(0),
            r"if \(!CraftExtended_WorldStateBufferSizeWithTail\(.*?\)\) \{\s*FeDiskIo_FatalError\(0\);\s*return;",
            "allocation-size failure must return after FeDiskIo_FatalError because it is not noreturn",
            re.S,
        )

    if save_start >= 0 and save_end >= 0:
        failures += require(
            save_body,
            r"!CraftExtended_WriteWorldStateTail\(.*?\)\) \{\s*FeDiskIo_FatalError\(0\);\s*return 0;",
            "XWES append failure must return after FeDiskIo_FatalError",
            re.S,
        )
        failures += require(
            save_body,
            r"> \(size_t\)INT_MAX\) \{\s*FeDiskIo_FatalError\(0\);\s*return 0;",
            "oversized serialized world state must return after FeDiskIo_FatalError",
            re.S,
        )

    # Recording and playback each have a temporary world-state allocation path in addition to the normal allocator.
    if text.count("CraftExtended_WorldStateBufferSizeWithTail(") < 3:
        failures.append("all three world-state allocation sites must include XWES worst-case capacity")

    allocation_failure_blocks = re.findall(
        r"if \(!CraftExtended_WorldStateBufferSizeWithTail\(.*?\)\) \{(.*?)\}",
        text,
        re.S,
    )
    if len(allocation_failure_blocks) != 3:
        failures.append("expected exactly three checked XWES allocation failure blocks")
    else:
        for index, block in enumerate(allocation_failure_blocks, 1):
            if re.search(r"FeDiskIo_FatalError\(0\);\s*return;", block, re.S) is None:
                failures.append(f"XWES allocation failure block {index} must return after FeDiskIo_FatalError")

    # Playback must publish the size read from the film before calling RestoreWorldState.
    playback = re.search(r"Film_ReadBytes\(&loopIdx,\s*sizeof\(loopIdx\)\);.*?Flight_RestoreWorldState\(\);", text, re.S)
    if playback is None:
        failures.append("missing film initial world-state restore path")
    else:
        pbody = playback.group(0)
        failures += require(pbody, r"worldStateSize\s*=\s*\(int\)loopIdx\s*;", "film playback must set worldStateSize from serialized world-state length")
        failures += require(pbody, r"g_worldStateSize\s*=\s*worldStateSize\s*;", "film playback must keep g_worldStateSize consistent before restore")
        failures += require(
            pbody,
            r"loopIdx > \(uint32_t\)INT_MAX\) \{\s*FeDiskIo_FatalError\(0\);\s*return;",
            "film world-state size overflow must return after FeDiskIo_FatalError",
            re.S,
        )

    if failures:
        print(f"FAIL: {len(failures)} Task 5 Flight/XWES integration contract violations")
        for failure in failures:
            print(f"- {failure}")
        return 1
    print("PASS: Flight world-state preserves classic prefix and integrates optional XWES tail")
    return 0


if __name__ == "__main__":
    sys.exit(main())
