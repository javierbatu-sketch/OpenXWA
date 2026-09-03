#!/usr/bin/env python3
"""Lock the recovered retail alias: meshRotation[componentId+49] is componentHp[componentId-1]."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
PATH = ROOT / "src/xwa/flight/object/collision.c"
text = PATH.read_text(encoding="utf-8", errors="strict")

bad = "targetCraft->meshRotation[(uint16_t)componentId + 49]"
expected = "CraftExtended_GetComponentHp(targetCraft, (uint16_t)componentId - 1)"

if bad in text or expected not in text:
    print("FAIL: collision AccelRing alias is not expressed as componentHp[componentId-1]")
    sys.exit(1)
print("PASS: collision AccelRing alias is expressed as componentHp[componentId-1]")
