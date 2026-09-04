#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
snapshot_c = (root / "src/xwa_runtime/snapshot/snapshot.c").read_text(encoding="utf-8")
ship_h = (root / "src/xwa_remaster/ship.h").read_text(encoding="utf-8")
ship_c = (root / "src/xwa_remaster/ship.c").read_text(encoding="utf-8")
preview_c = (root / "src/xwa_remaster/preview.c").read_text(encoding="utf-8")

errors = []
for raw in ("cd->componentState", "cd->meshRotation", "cd->componentHp"):
    if raw in snapshot_c:
        errors.append(f"snapshot.c still reads raw retail craft array: {raw}")
if '"xwa/flight/object/craft_extended_state.h"' not in snapshot_c:
    errors.append("snapshot.c does not include authoritative craft_extended_state accessors")
for accessor in (
    "CraftExtended_GetMeshComponentState(cd, meshIndex)",
    "CraftExtended_GetMeshRotation(cd, meshIndex)",
    "CraftExtended_GetComponentHp(cd, meshIndex)",
):
    if accessor not in snapshot_c:
        errors.append(f"snapshot.c missing authoritative mesh capture accessor: {accessor}")
if "meshIndex < XWA_SNAP_MAX_MESH_SLOTS" not in snapshot_c:
    errors.append("snapshot.c does not capture exactly the renderable snapshot mesh range")
if "XwaSnapshot_EngineKnockoutSet(f->eg_knockout_mask, k->emitterIndex)" not in snapshot_c:
    errors.append("snapshot.c does not publish high emitter knockout bits")
if "uint32_t knockout_mask, float scale" in ship_h or "uint32_t knockout_mask, float scale" in ship_c:
    errors.append("remaster engine-glow consumer still accepts only a single 32-bit knockout mask")
if "/*knockout_mask=*/NULL" not in preview_c:
    errors.append("preview does not pass the explicit no-knockout 256-bit mask sentinel")

if "special_component_state" in snapshot_c or "special_component_state" in ship_h or "special_component_state" in ship_c:
    errors.append("snapshot/remaster exposes raw special component state instead of presentation-derived state")
if "f->damage_flame_frame = CraftExtended_GetSpecialComponentState(cd);" not in snapshot_c:
    errors.append("snapshot.c does not derive damage-flame presentation state from special component 254")
for remaster_path in (root / "src/xwa_remaster/flight.c", root / "src/xwa_remaster/hud_cmd.c"):
    remaster_text = remaster_path.read_text(encoding="utf-8")
    if "component_state[49]" in remaster_text:
        errors.append(f"{remaster_path.name} still aliases mesh 49 as the retail special damage-flame state")
    if "damage_flame_frame" not in remaster_text:
        errors.append(f"{remaster_path.name} does not consume the dedicated damage-flame snapshot field")

if errors:
    raise SystemExit("\n".join(errors))
print("xwau extended snapshot guard: GREEN")
