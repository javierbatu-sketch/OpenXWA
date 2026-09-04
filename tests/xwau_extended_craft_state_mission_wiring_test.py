#!/usr/bin/env python3
from pathlib import Path
import sys

repo = Path(__file__).resolve().parents[1]
mission = (repo / "src/xwa/flight/mission/mission.c").read_text(encoding="utf-8")

errors = []

if '#include "xwa/flight/object/craft_extended_state.h"' not in mission:
    errors.append("mission.c does not include craft_extended_state.h")

free_start = mission.find("void Mission_FreeObjectStorageHandles(void)")
init_start = mission.find("uint16_t Mission_Init(char* fileName)")
if free_start < 0 or init_start < 0:
    errors.append("could not locate mission storage lifecycle functions")
else:
    free_body = mission[free_start:init_start]
    if "CraftExtended_Free();" not in free_body:
        errors.append("Mission_FreeObjectStorageHandles does not free the sidecar")

    next_function = mission.find("// FUNCTION: XWA", init_start + 1)
    init_body = mission[init_start: next_function if next_function >= 0 else len(mission)]
    if "CraftExtended_Free();" not in init_body:
        errors.append("Mission_Init does not release the previous sidecar before storage recreation")
    alloc = init_body.find("CraftExtended_Allocate(g_craftObjectSlotsTotal)")
    lock = init_body.find("g_craftDataPoolBase = (CraftData*)Memory_LockHandle(g_craftDataPoolHandle)")
    if alloc < 0:
        errors.append("Mission_Init does not allocate the sidecar with g_craftObjectSlotsTotal")
    elif lock < 0 or alloc < lock:
        errors.append("sidecar allocation must occur after the CraftData pool base is locked")
    if alloc >= 0:
        tail = init_body[alloc:alloc + 180]
        if "return 0;" not in tail:
            errors.append("Mission_Init does not fail explicitly when sidecar allocation fails")

if errors:
    print("FAIL: XWAU craft sidecar mission lifecycle is not wired")
    for error in errors:
        print(f"- {error}")
    sys.exit(1)

print("PASS: XWAU craft sidecar mission lifecycle wiring")
