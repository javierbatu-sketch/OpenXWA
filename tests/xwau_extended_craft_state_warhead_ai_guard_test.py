#!/usr/bin/env python3
"""Reject raw warheadData access in the flight AI subsystem."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
AI_ROOT = ROOT / "src" / "xwa" / "flight" / "ai"
TOKEN = re.compile(r"->\s*warheadData\b")

def main() -> int:
    violations = []
    for path in sorted(AI_ROOT.rglob("*.c")) + sorted(AI_ROOT.rglob("*.h")):
        text = path.read_text(encoding="utf-8", errors="strict")
        lines = text.splitlines()
        rel = path.relative_to(ROOT)
        for match in TOKEN.finditer(text):
            line = text.count("\n", 0, match.start()) + 1
            violations.append((str(rel), line, lines[line - 1].strip()))
    if violations:
        print(f"FAIL: {len(violations)} unauthorized raw AI warheadData references")
        for rel, line, excerpt in violations:
            print(f"{rel}:{line}: {excerpt}")
        return 1
    print("PASS: no unauthorized raw AI warheadData references")
    return 0

if __name__ == "__main__":
    sys.exit(main())
