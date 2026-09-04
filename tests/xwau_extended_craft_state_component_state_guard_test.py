#!/usr/bin/env python3
"""Task-4 gate: no raw mission/runtime componentState access outside explicit compatibility boundaries."""

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = ROOT / "src" / "xwa"
TOKEN = re.compile(r"->\s*componentState\b", re.MULTILINE)
ALLOWED_FILES = {
    Path("src/xwa/flight/object/craft_extended_state.c"),
    Path("src/xwa/render/render_scene_preview.c"),
}


def line_number(text: str, pos: int) -> int:
    return text.count("\n", 0, pos) + 1


def main() -> int:
    violations = []
    for path in sorted(SOURCE_ROOT.rglob("*.c")):
        rel = path.relative_to(ROOT)
        if rel in ALLOWED_FILES:
            continue
        text = path.read_text(encoding="utf-8", errors="strict")
        lines = text.splitlines()
        for match in TOKEN.finditer(text):
            line = line_number(text, match.start())
            violations.append((str(rel), line, lines[line - 1].strip()))

    if violations:
        print(f"FAIL: {len(violations)} unauthorized raw componentState references")
        for rel, line, excerpt in violations:
            print(f"{rel}:{line}: {excerpt}")
        return 1

    print("PASS: no unauthorized raw componentState references")
    return 0


if __name__ == "__main__":
    sys.exit(main())
