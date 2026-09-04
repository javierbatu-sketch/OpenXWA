#!/usr/bin/env python3
"""Reject raw warheadData access in laser.c."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
TARGET = ROOT / "src" / "xwa" / "flight" / "object" / "laser.c"
TOKEN = re.compile(r"->\s*warheadData\b")


def main() -> int:
    text = TARGET.read_text(encoding="utf-8", errors="strict")
    lines = text.splitlines()
    violations = []
    for match in TOKEN.finditer(text):
        line = text.count("\n", 0, match.start()) + 1
        violations.append((line, lines[line - 1].strip()))
    if violations:
        print(f"FAIL: {len(violations)} unauthorized raw laser.c warheadData references")
        for line, excerpt in violations:
            print(f"src/xwa/flight/object/laser.c:{line}: {excerpt}")
        return 1
    print("PASS: no unauthorized raw laser.c warheadData references")
    return 0


if __name__ == "__main__":
    sys.exit(main())
