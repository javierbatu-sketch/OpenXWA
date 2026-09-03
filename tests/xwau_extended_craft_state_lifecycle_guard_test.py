#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]

requirements = {
    Path('src/xwa/flight/object/object.c'): [
        'CraftExtended_Copy(g_objectTable[dstObjIdx].mobj->pCraft,',
        'CraftExtended_ClearLegacyReusablePrefix(g_objectTable[objIdx].mobj->pCraft);',
        'CraftExtended_ResetCraft(g_objectTable[candidateObjIdx].mobj->pCraft);',
        'CraftExtended_ResetComponentArrays(craft);',
    ],
    Path('src/xwa/flight/object/debris.c'): [
        'CraftExtended_ResetCraft(g_objectTable[newObjIdx].mobj->pCraft);',
    ],
    Path('src/xwa/flight/death_star.c'): [
        'CraftExtended_ResetCraft(craft);',
    ],
    Path('src/xwa/flight/mission/mission.c'): [
        'CraftExtended_ResetCraft(g_objectTable[objIdx].mobj->pCraft);',
        'CraftExtended_ResetWeaponState(g_objectTable[objIdx].mobj->pCraft);',
        'CraftExtended_ResetComponentArrays(g_curCraft);',
    ],
    Path('src/xwa/flight/yard.c'): [
        'CraftExtended_ResetComponentArrays(g_curCraft);',
    ],
    Path('src/xwa/flight/hangar.c'): [
        'CraftExtended_ResetComponentArrays(g_curCraft);',
    ],
}

missing = []
for rel, needles in requirements.items():
    text = (ROOT / rel).read_text(encoding='utf-8')
    for needle in needles:
        if needle not in text:
            missing.append(f'{rel}: missing {needle}')

if missing:
    print('FAIL: extended craft lifecycle owners are incomplete')
    for item in missing:
        print(item)
    sys.exit(1)

print('PASS: extended craft lifecycle owners cover copy/reset/reuse boundaries')
