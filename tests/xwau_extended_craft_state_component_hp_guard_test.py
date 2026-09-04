#!/usr/bin/env python3
"""Reject raw mission/runtime componentHp access outside compatibility/preview boundaries."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = ROOT / "src" / "xwa"
TOKEN = re.compile(r"->\s*componentHp\b")
ALLOWED_FILES = {
    Path("src/xwa/flight/object/craft_extended_state.c"),
    Path("src/xwa/render/render_scene_preview.c"),
}


def main() -> int:
    violations = []
    for path in sorted(SOURCE_ROOT.rglob("*.c")) + sorted(SOURCE_ROOT.rglob("*.h")):
        rel = path.relative_to(ROOT)
        if rel in ALLOWED_FILES:
            continue
        text = path.read_text(encoding="utf-8", errors="strict")
        lines = text.splitlines()
        for match in TOKEN.finditer(text):
            line = text.count("\n", 0, match.start()) + 1
            violations.append((str(rel), line, lines[line - 1].strip()))
    if violations:
        print(f"FAIL: {len(violations)} unauthorized raw componentHp references")
        for rel, line, excerpt in violations:
            print(f"{rel}:{line}: {excerpt}")
        return 1
    print("PASS: no unauthorized raw componentHp references")
    return 0

if __name__ == "__main__":
    sys.exit(main())
