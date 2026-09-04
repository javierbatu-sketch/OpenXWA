#!/usr/bin/env python3
"""Reject raw warheadData access in mission.c and flight.c."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
TARGETS = (
    ROOT / "src/xwa/flight/mission/mission.c",
    ROOT / "src/xwa/flight/flight.c",
)
TOKEN = re.compile(r"->\s*warheadData\b")

def main() -> int:
    violations = []
    for path in TARGETS:
        text = path.read_text(encoding="utf-8")
        lines = text.splitlines()
        for match in TOKEN.finditer(text):
            line = text.count("\n", 0, match.start()) + 1
            violations.append((path.relative_to(ROOT), line, lines[line - 1].strip()))
    if violations:
        print(f"FAIL: {len(violations)} unauthorized raw mission/flight warheadData references")
        for rel, line, excerpt in violations:
            print(f"{rel}:{line}: {excerpt}")
        return 1
    print("PASS: no unauthorized raw mission/flight warheadData references")
    return 0

if __name__ == "__main__":
    sys.exit(main())
