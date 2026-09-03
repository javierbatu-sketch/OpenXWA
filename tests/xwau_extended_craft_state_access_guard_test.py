#!/usr/bin/env python3
"""Reject raw gameplay access to CraftData arrays owned by the XWAU sidecar."""

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = ROOT / "src" / "xwa"
FIELDS = (
    "componentState",
    "meshRotation",
    "componentHp",
    "engineEmitterHealth",
    "warheadData",
)
TOKEN = re.compile(r"->\s*(" + "|".join(FIELDS) + r")\b", re.MULTILINE)

# The sidecar/compatibility implementation is allowed to project the retail arrays.
# render_scene_preview.c is the one explicit non-pool exception: it renders the standalone
# g_modelPreviewCraftData owned by the frontend and cannot use craft-pool ordinal mapping.
# No mission gameplay source may be allowlisted.
ALLOWED_FILES = {
    Path("src/xwa/flight/object/craft_extended_state.c"),
    Path("src/xwa/render/render_scene_preview.c"),
}


def line_number(text: str, pos: int) -> int:
    return text.count("\n", 0, pos) + 1


def main() -> int:
    violations = []
    source_paths = sorted(SOURCE_ROOT.rglob("*.c")) + sorted(SOURCE_ROOT.rglob("*.h"))
    for path in source_paths:
        rel = path.relative_to(ROOT)
        if rel in ALLOWED_FILES:
            continue
        text = path.read_text(encoding="utf-8", errors="strict")
        for match in TOKEN.finditer(text):
            line = line_number(text, match.start())
            excerpt = text.splitlines()[line - 1].strip()
            violations.append((str(rel), line, match.group(1), excerpt))

    if violations:
        print(f"FAIL: {len(violations)} unauthorized raw CraftData array references")
        for rel, line, field, excerpt in violations:
            print(f"{rel}:{line}: {field}: {excerpt}")
        return 1

    print("PASS: no unauthorized raw CraftData extended-state array references")
    return 0


if __name__ == "__main__":
    sys.exit(main())
