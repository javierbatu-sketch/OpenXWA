# XWAU-004 RED5 Legacy Material Shading Implementation Plan

> **For agentic execution:** use the superpowers TDD and execution workflow task-by-task. Do not write production code until the corresponding RED test has been run and observed failing for the intended missing behavior.

**Goal:** Carry the reference-verified XWA Upgrade static material shading core from OpenXWA's `.mat` parser through the current Aeron OPT/glTF/material-buffer path and render it in the existing mesh shader without translating XWAU `Glossiness` or `Metallic` into glTF PBR metallic/roughness.

**Architecture:** OpenXWA remains the owner of XWA/XWAU semantics and configuration. It loads `Materials/<OPT>.mat`, applies the reference-verified XWAU material defaults and authored overrides, loads the four relevant global controls from `SSAO.cfg`, resolves DAT-backed `NormalMap` images with the existing `Xwa2d` DAT codec, and emits renderer-generic material overrides. Aeron receives only generic effective legacy-material controls, routes them through the same OPT → cgltf → cooker → `AeronGltfMaterial` → mesh GPU material buffer path used today, and selects a per-material legacy shading branch in the existing mesh shader. There is no second renderer, no XWAU parser inside Aeron, and no reuse of the old DMJC dynamic-map API.

**Tech stack:** C99/C11, current Aeron C API, cgltf/OPT converter, HLSL, Python contract tests, GitHub Actions, OpenXWA `Xwa2d` DAT codec, Aeron VFS.

**Approved spec:** `docs/superpowers/specs/2026-09-01-xwau-004-static-materials-design.md`

## Global constraints

- Physical project sources are authoritative for XWAU semantics: supplied `XwaUPGRADE.zip` / `ddraw_effects.dll`, `SSAO.cfg`, `Materials.zip`, `Effects.zip`.
- OpenXWA/Aeron source is then used to fit those semantics into the current architecture.
- Never implement `Glossiness -> 1 - roughness`.
- Never feed XWAU `Metallic` to glTF/Aeron PBR `metallic_factor`.
- Preserve XWAU authored values outside 0..1. Legacy numeric fields validate finite-only.
- Existing generic RED4 PBR metallic/roughness overrides remain supported and keep their existing 0..1 validation.
- Missing `.mat` keeps current OpenXWA/Aeron material behavior, as approved by the spec.
- When a `.mat` is present and legacy shading is produced, seed the legacy core with the reference material constructor values demonstrated in `ddraw_effects.dll` before overlaying authored values:
  - Metallic = `0.3`
  - Intensity = `0.25`
  - Glossiness = `0.02`
  - NMIntensity = `0.5`
  - SpecularVal = `0.0`
  - Ambient = `0.0`
  - Shadeless = false
- Read the shipped XWAU global material controls from asset-root `SSAO.cfg`:
  - `specular_intensity = 0.5`
  - `glossiness = 128.0`
  - `lightness_boost = 8.0`
  - `saturation_boost = 1.0`
  Those values are fallbacks only when the key/file is absent; an authored finite value overrides them.
- OpenXWA translates XWAU values into renderer-generic effective fields. Aeron must not know `SSAO.cfg`.
- `NoBloom` and `AlphaIsntGlass` remain explicit follow-up work and are not approximated in RED5.
- `SpecularMap`/`SpecularMapIntensity`, dynamic/event materials, Dynamic Cockpit and Active Cockpit remain out of RED5.
- Never merge to `xwau-support` until focused tests, full Aeron/OpenXWA CI and real XWAU runtime/resource validation are all green.

## Reference math fixed by project sources

The following math is the RED5 reference contract and comes from the supplied `ddraw_effects.dll` DXBC plus `SSAO.cfg`, not from PBR naming guesses.

OpenXWA computes effective scalar inputs:

```text
legacy_specular_exponent =
    max(authored_glossiness * ssao.glossiness, 0.05)

legacy_specular_intensity =
    authored_intensity * ssao.specular_intensity

legacy_specular_color_control = authored_metallic
legacy_specular_value = authored_specular_val
legacy_ambient = authored_ambient
normal_scale = authored_nm_intensity
legacy_lightness_boost = ssao.lightness_boost
legacy_saturation_boost = ssao.saturation_boost
```

The shader's legacy specular-color helper uses the base-color texel converted to HSV:

```text
m = legacy_specular_color_control

if m < 1.1:
    specS = baseS * m * legacy_saturation_boost
    specV = baseV * legacy_lightness_boost
          + legacy_specular_value * (1.0 - m)
    specColor = HSVtoRGB(baseH, specS, specV)
    diffuseMetalScale = 1.0 - 2.0 * m
else:
    specColor = float3(1.0, 1.0, 1.0)
    diffuseMetalScale = 1.0
```

The normal-map strength contract is:

```text
N = normalize(N_geom + normal_scale * (N_mapped - N_geom))
```

`normal_scale` is not clamped; values above 1 extrapolate as the reference shader does.

The legacy specular lobe uses the effective exponent and intensity as separate controls:

```text
spec = pow(max(dot(N, H), 0.0), legacy_specular_exponent)
     * legacy_specular_intensity
```

Aeron continues to provide the current scene lights/environment. RED5 changes the material response, not scene ownership.

Material ambient brightening uses the demonstrated reference form:

```text
lit = lit + legacy_ambient * (baseColor - lit)
```

equivalently applying the material term as interpolation/extrapolation toward fully lit base color. Do not clamp `legacy_ambient` merely because it is outside 0..1.

`Shadeless` bypasses the normal lighting response for that material while preserving base texture/color, alpha handling and the already-existing emissive composition path.

---

## File map

### Aeron — modify

- `include/aeron/asset/opt_model.h`
  - Extend the generic OPT material-override payload.
- `src/asset/opt_model.c`
  - Validate/copy the new generic fields into opt2gltf options.
- `tools/opt2gltf/opt_material_override.h`
- `tools/opt2gltf/opt_material_override.c`
  - Resolve one optional default override plus one specific texture override.
- `tools/opt2gltf/opt2gltf.c`
  - Attach generic legacy metadata and optional owned normal image to the existing cgltf material.
- `tools/opt2gltf/opt2gltf.h`
  - Carry expanded override payload.
- `include/aeron/scene/gltf_mesh.h`
  - Carry parsed generic legacy fields.
- `src/scene/gltf_mesh.c`
  - Parse generated Aeron material extras into `AeronGltfMaterial`.
- `include/aeron/scene/mesh.h`
  - Expand the GPU material record from 128 to 160 bytes.
- `src/scene/mesh.c`
  - Populate legacy vectors/flags.
- `shaders/scene_pbr_material_alpha.hlsli`
  - Mirror the 160-byte material layout and new flags.
- `shaders/scene_pbr_mesh_impl.hlsli`
  - Existing material path selects PBR vs legacy behavior per material.
- `.github/workflows/xwau-material-overrides-test.yml`
  - Extend focused RED5 coverage.

### Aeron — add tests

- `tests/opt_model_legacy_material_api_test.c`
- `tests/opt_material_legacy_override_test.c`
- `tests/opt_material_legacy_propagation_test.py`
- `tests/gltf_legacy_material_contract_test.py`
- `tests/mesh_legacy_material_layout_test.c`
- `tests/legacy_material_shader_contract_test.py`

### OpenXWA — add

- `src/xwa_remaster/xwau_material_render.h`
- `src/xwa_remaster/xwau_material_render.c`
  - XWAU constructor defaults + `SSAO.cfg` static controls + translation to generic Aeron values.
- `src/xwa_remaster/xwau_normal_map.h`
- `src/xwa_remaster/xwau_normal_map.c`
  - Parse `Effects\Name.dat-G-F`, VFS-read DAT, reuse `Xwa2d_DatAppendGroup`, select sprite ID.
- `tests/xwau_material_render_test.c`
- `tests/xwau_normal_map_test.c`
- `tests/xwau_opt_material_integration_test.py`

### OpenXWA — modify

- `src/xwa_remaster/opt_mesh.c`
  - Load `.mat`, build default/named overrides, keep normal pixels alive for the synchronous Aeron build, free afterward.
- `CMakeLists.txt`
  - Add new remaster source files to the existing target.
- `.github/workflows/xwau-static-material-test.yml`
  - Extend focused XWAU-004 coverage.

---

# Task 1: RED5-A — Aeron public generic legacy-material API

**Files:**
- Modify: `aeron/tests/opt_model_legacy_material_api_test.c` (new)
- Modify: `aeron/.github/workflows/xwau-material-overrides-test.yml`

### Step 1: Write the failing public-API test

Create `tests/opt_model_legacy_material_api_test.c` that includes `aeron/asset/opt_model.h` and instantiates one `AeronOptMaterialOverride` with all future generic fields.

The test must require these flags:

```c
AERON_OPT_MATERIAL_OVERRIDE_LEGACY_SPECULAR_EXPONENT
AERON_OPT_MATERIAL_OVERRIDE_LEGACY_SPECULAR_INTENSITY
AERON_OPT_MATERIAL_OVERRIDE_LEGACY_SPECULAR_COLOR_CONTROL
AERON_OPT_MATERIAL_OVERRIDE_LEGACY_SPECULAR_VALUE
AERON_OPT_MATERIAL_OVERRIDE_LEGACY_AMBIENT
AERON_OPT_MATERIAL_OVERRIDE_NORMAL_SCALE
AERON_OPT_MATERIAL_OVERRIDE_LEGACY_LIGHTNESS_BOOST
AERON_OPT_MATERIAL_OVERRIDE_LEGACY_SATURATION_BOOST
AERON_OPT_MATERIAL_OVERRIDE_LEGACY_SHADELESS
AERON_OPT_MATERIAL_OVERRIDE_NORMAL_IMAGE
```

Require this generic image view:

```c
typedef struct AeronOptMaterialImage {
    const uint8_t* rgba8;
    uint32_t width;
    uint32_t height;
} AeronOptMaterialImage;
```

Require these payload fields on `AeronOptMaterialOverride`:

```c
float legacy_specular_exponent;
float legacy_specular_intensity;
float legacy_specular_color_control;
float legacy_specular_value;
float legacy_ambient;
float normal_scale;
float legacy_lightness_boost;
float legacy_saturation_boost;
bool legacy_shadeless;
AeronOptMaterialImage normal_image;
```

The test also instantiates an override with `texture_name = NULL`; RED5 defines that as the one optional generic default applied before a specific texture override.

### Step 2: Add the workflow compile step

Add:

```yaml
- name: Build legacy material public RED contract
  shell: bash
  run: |
    gcc -std=c11 -Wall -Wextra -Werror -Iinclude \
      -fsyntax-only tests/opt_model_legacy_material_api_test.c
```

and include the new file in `pull_request.paths`.

### Step 3: Verify RED

Run/push the RED commit so the focused workflow executes.

Expected failure: unknown legacy flags/type fields in `include/aeron/asset/opt_model.h`. Existing RED4 material and atlas steps must still pass.

### Step 4: Commit RED only

```bash
git add tests/opt_model_legacy_material_api_test.c .github/workflows/xwau-material-overrides-test.yml
git commit -m "test: add legacy OPT material API RED contract"
```

Do not edit production headers in this commit.

---

# Task 2: GREEN5-A — Generic override transport and default/specific merge

**Files:**
- Modify: `aeron/include/aeron/asset/opt_model.h`
- Modify: `aeron/src/asset/opt_model.c`
- Modify: `aeron/tools/opt2gltf/opt_material_override.h`
- Modify: `aeron/tools/opt2gltf/opt_material_override.c`
- Modify: `aeron/tools/opt2gltf/opt2gltf.h`
- Add: `aeron/tests/opt_material_legacy_override_test.c`
- Modify: `aeron/tests/opt_model_material_overrides_propagation_test.py`
- Modify workflow.

### Step 1: Add a second RED behavior test before each production behavior

`opt_material_legacy_override_test.c` must cover real helper behavior:

1. A `texture_name == NULL` default applies to every texture.
2. One named specific override overlays the default for the matching real texture name.
3. Generated `TexNN` alias remains ASCII-case-insensitive.
4. Two default records are an error.
5. Two specific records matching the same texture remain an error (RED4 behavior preserved).
6. A partial specific override leaves default fields unchanged.
7. Legacy floats above 1.0 survive unchanged.
8. Non-finite legacy floats are rejected.
9. Existing PBR metallic/roughness still reject outside 0..1.
10. `legacy_shadeless` is carried as a boolean presence/value, not inferred from a float.

Run it before production changes and confirm it fails because the new fields/default semantics do not exist.

### Step 2: Implement the public/internal payload minimally

Append the new flag bits after RED4's existing bits. Do not renumber the existing metallic/roughness bits.

Mirror the public fields in `OptGltfMaterialOverride`.

Add an internal resolver:

```c
bool OptGltf_ResolveMaterialOverride(
    const OptGltfMaterialOverride* overrides,
    size_t override_count,
    const char* texture_name,
    size_t texture_index,
    OptGltfMaterialOverride* out,
    bool* out_has_override,
    opt_error_t* error);
```

Rules:
- zero `out`;
- merge at most one `texture_name == NULL` default;
- find at most one specific match by current real-name / TexNN logic;
- overlay only flagged fields;
- duplicate default or duplicate specific is an explicit conversion error.

`OptGltf_ApplyMaterialOverrides()` may call the resolver and retain its existing PBR application behavior so RED4 callers/tests stay compatible.

### Step 3: Validate at the public Aeron boundary

In `src/asset/opt_model.c`:
- existing PBR factors: finite and 0..1;
- new legacy numeric fields: finite-only;
- normal image if flagged: non-null RGBA and width/height > 0;
- copy all bits/fields exactly to internal options.

### Step 4: Verify GREEN

Focused tests:
- existing atlas tests green;
- existing RED4 tests green;
- new public API test green;
- new merge/finite-value test green;
- propagation Python test green.

### Step 5: Commit

```bash
git add include/aeron/asset/opt_model.h src/asset/opt_model.c \
        tools/opt2gltf/opt_material_override.h tools/opt2gltf/opt_material_override.c \
        tools/opt2gltf/opt2gltf.h tests/opt_material_legacy_override_test.c \
        tests/opt_model_material_overrides_propagation_test.py \
        .github/workflows/xwau-material-overrides-test.yml
git commit -m "feat: add generic legacy OPT material transport"
```

---

# Task 3: RED5-B — cgltf metadata and normal-image channel

**Files:**
- Add: `aeron/tests/gltf_legacy_material_contract_test.py`
- Modify workflow.

### Step 1: Write the failing conversion contract

The Python test inspects the current conversion sources and requires:

- `OptGltf_ResolveMaterialOverride()` is used for each OPT texture.
- A flagged `normal_image` produces an additional owned cgltf image/texture and assigns it to the material's existing `normal_texture` channel.
- The owned image is copied into `OptGltfDocument::image_pixels`; it is not a borrowed pointer that can outlive OpenXWA's temporary frame.
- The generated URI is deterministic:
  `textures/<basename>_TexNN_normal.png`.
- Legacy scalar metadata is emitted in one Aeron-owned material `extras` object under `aeronLegacyMaterial`.
- Existing `aeronEmissiveMode` can coexist in the same JSON object; it is not overwritten.
- Metadata keys are generic, exactly:
  - `specularExponent`
  - `specularIntensity`
  - `specularColorControl`
  - `specularValue`
  - `ambient`
  - `normalScale`
  - `lightnessBoost`
  - `saturationBoost`
  - `shadeless`
- No key contains `XWAU`, `Glossiness`, `Metallic`, `NoBloom` or `AlphaIsntGlass`.

### Step 2: Verify RED

Expected failure: normal override image/legacy metadata are not emitted yet.

### Step 3: Commit RED

```bash
git add tests/gltf_legacy_material_contract_test.py .github/workflows/xwau-material-overrides-test.yml
git commit -m "test: add legacy glTF material RED contract"
```

---

# Task 4: GREEN5-B — Emit normal image and generic material extras

**Files:**
- Modify: `aeron/tools/opt2gltf/opt2gltf.c`
- If needed for cohesion only: add `aeron/tools/opt2gltf/opt_material_metadata.h/.c` and wire into CMake.

### Step 1: Resolve overrides once per source texture

Before final image allocation, count how many source textures resolve a `NORMAL_IMAGE` override. Include that count in `image_total`.

For each source texture:
- resolve default+specific override;
- preserve existing base/emissive image behavior;
- when normal-image flagged:
  - allocate/copy `width * height * 4` bytes;
  - add one cgltf image and texture;
  - bind the texture to the existing `cgltf_material.normal_texture`;
  - store copied pixels in `document->image_pixels`.

Reject byte-count overflow before allocation.

### Step 2: Emit one deterministic extras object

Because opt2gltf creates these materials itself, build one bounded JSON object after both emissive and legacy state are known.

Examples:

```json
{"aeronLegacyMaterial":{"specularExponent":2.56,"specularIntensity":0.125,
"specularColorControl":0.3,"specularValue":0.0,"ambient":0.0,
"normalScale":0.5,"lightnessBoost":8.0,"saturationBoost":1.0,"shadeless":false}}
```

or, when both features are present:

```json
{"aeronEmissiveMode":"legacy_srgb_srcalpha",
 "aeronLegacyMaterial":{...}}
```

Use locale-independent finite float serialization. The production helper must emit JSON numeric syntax with `.` regardless of process locale. Add a small focused C test if serialization is extracted to its own helper.

### Step 3: Verify GREEN

Run the conversion contract and existing RED4 tests. Build the affected converter target in CI so cgltf field usage is syntax/type checked.

### Step 4: Commit

```bash
git add tools/opt2gltf/opt2gltf.c tools/opt2gltf/opt_material_metadata.* \
        tests/gltf_legacy_material_contract_test.py \
        .github/workflows/xwau-material-overrides-test.yml CMakeLists.txt
git commit -m "feat: emit generic legacy OPT material data"
```

Only add `opt_material_metadata.*`/CMake if the helper is actually introduced.

---

# Task 5: RED5-C — Aeron material loader and CPU/GPU layout

**Files:**
- Add: `aeron/tests/mesh_legacy_material_layout_test.c`
- Add: `aeron/tests/legacy_material_metadata_parse_test.c` if a parser helper is extracted.
- Modify workflow.

### Step 1: Write layout RED

Require `AeronGltfMaterial` to expose:
- a legacy-material presence flag;
- the eight generic scalar fields;
- generic shadeless state.

Require `AeronPbrMaterialEntry` to be exactly 160 bytes and to retain the existing first 128 bytes conceptually unchanged, then append:

```c
float legacy0[4]; /* exponent, intensity, color-control, specular-value */
float legacy1[4]; /* ambient, normal-scale, lightness-boost, saturation-boost */
```

Shadeless and legacy-mode state live in `flags`, not in a third vector.

New GPU flag bits must not collide with `0x1..0x20`:
- `0x40` = legacy material data present
- `0x80` = legacy shadeless

Compile-time assertions test:
- `sizeof(AeronPbrMaterialEntry) == 160`
- offsets of `legacy0` and `legacy1`
- old flag values remain unchanged.

### Step 2: Write metadata parse RED

Given generated `cgltf_extras.data`, `Aeron_GltfMeshBuildData()` must populate all eight fields and shadeless exactly. A material without `aeronLegacyMaterial` must leave legacy mode false and retain current PBR values.

Malformed/non-finite Aeron-generated legacy metadata is a build error, not silently accepted.

### Step 3: Verify RED and commit

```bash
git add tests/mesh_legacy_material_layout_test.c \
        tests/legacy_material_metadata_parse_test.c \
        .github/workflows/xwau-material-overrides-test.yml
git commit -m "test: add legacy GPU material RED contract"
```

---

# Task 6: GREEN5-C — Parse metadata and populate the 160-byte material record

**Files:**
- Modify: `aeron/include/aeron/scene/gltf_mesh.h`
- Modify: `aeron/src/scene/gltf_mesh.c`
- Modify: `aeron/include/aeron/scene/mesh.h`
- Modify: `aeron/src/scene/mesh.c`
- Modify: `aeron/shaders/scene_pbr_material_alpha.hlsli`

### Step 1: Extend `AeronGltfMaterial`

Add the generic legacy values and flags. Keep ordinary glTF fields untouched.

Parse only the Aeron-owned `aeronLegacyMaterial` object produced by opt2gltf. Reuse the existing extras-reading path used for `aeronEmissiveMode`; if extraction into a bounded helper makes the code testable, do that rather than duplicating string scans.

### Step 2: Expand CPU and HLSL layouts together

CPU:

```c
typedef struct AeronPbrMaterialEntry {
    /* existing 128 bytes unchanged */
    ...
    uint32_t flags;
    uint32_t _pad[3];
    float legacy0[4];
    float legacy1[4];
} AeronPbrMaterialEntry;
```

HLSL must mirror exact order/types.

Population:

```text
legacy0 = {
  specularExponent,
  specularIntensity,
  specularColorControl,
  specularValue
}

legacy1 = {
  ambient,
  normalScale,
  lightnessBoost,
  saturationBoost
}
```

Set `0x40` only when generic legacy metadata exists. Set `0x80` only when legacy shadeless is true.

### Step 3: Verify GREEN

- layout test green;
- metadata parse test green;
- existing mesh/material tests green;
- shader compilation/build in normal Aeron CI must still accept the new 160-byte layout.

### Step 4: Commit

```bash
git add include/aeron/scene/gltf_mesh.h src/scene/gltf_mesh.c \
        include/aeron/scene/mesh.h src/scene/mesh.c \
        shaders/scene_pbr_material_alpha.hlsli \
        tests/mesh_legacy_material_layout_test.c \
        tests/legacy_material_metadata_parse_test.c
git commit -m "feat: carry legacy material data to GPU"
```

---

# Task 7: RED5-D — Shader behavior contract

**Files:**
- Add: `aeron/tests/legacy_material_shader_contract_test.py`
- Modify workflow.

### Step 1: Write source/compile RED

The contract requires the existing mesh shader to:
- define flag constants `0x40` and `0x80`;
- keep the old PBR branch present for materials without `0x40`;
- decode `legacy0`/`legacy1`;
- implement the exact non-clamped normal-strength equation;
- include RGB↔HSV helper logic for legacy specular color;
- include the `1.1` color-control threshold;
- use `specularExponent` in `pow(max(dot(N,H),0), exponent)`;
- multiply by separate `specularIntensity`;
- apply ambient toward fully lit base color;
- branch shadeless before regular lighting while retaining existing alpha/emissive handling.

The workflow must also build/compile the actual Aeron shader target, so the test is not merely a text search.

### Step 2: Verify RED

Expected failure: the legacy GPU fields are not consumed by the current shader.

### Step 3: Commit RED

```bash
git add tests/legacy_material_shader_contract_test.py \
        .github/workflows/xwau-material-overrides-test.yml
git commit -m "test: add legacy material shader RED contract"
```

---

# Task 8: GREEN5-D — Existing Aeron mesh shader gains legacy branch

**Files:**
- Modify: `aeron/shaders/scene_pbr_mesh_impl.hlsli`
- Optionally add: `aeron/shaders/scene_legacy_material.hlsli` if extracting pure helpers makes the branch clearer.

### Step 1: Preserve the normal PBR path byte-for-byte where practical

Resolve material flags first.

For ordinary materials (`legacy flag == 0`), retain the current PBR normal, roughness/metallic, lighting, alpha and emissive logic.

### Step 2: Apply legacy normal-map strength only to legacy materials

Compute current `N_mapped` exactly as today, then:

```hlsl
N = normalize(N_geom + normalScale * (N_mapped - N_geom));
```

If there is no normal texture, keep `N_geom`.

Do not `saturate(normalScale)`.

### Step 3: Implement reference legacy specular color

Use the Reference math section above exactly.

Do not substitute glTF metallic, dielectric F0 or roughness.

### Step 4: Integrate with current Aeron lights

For the existing light/view/half vectors:
- diffuse uses the existing scene/light ownership;
- specular lobe uses legacy exponent and intensity;
- specular color comes from the reference HSV material helper;
- material ambient brightens toward fully lit base color;
- shadeless returns the base material response without the ordinary lighting multiplication.

Keep existing alpha-mode behavior and existing emissive composition after the legacy/PBR base-lighting branch.

Do not implement per-material bloom exclusion here.

### Step 5: Verify GREEN

Run focused contract plus actual shader build and existing Aeron CI tests.

### Step 6: Commit

```bash
git add shaders/scene_pbr_mesh_impl.hlsli shaders/scene_legacy_material.hlsli \
        tests/legacy_material_shader_contract_test.py
git commit -m "feat: render generic legacy materials"
```

Only add the helper file if used.

---

# Task 9: Aeron RED5 checkpoint verification

No new production behavior.

### Step 1: Run focused workflow on exact Aeron head

Required focused steps:
- RED4 atlas/material override regression
- RED5 public API
- default/specific merge
- glTF metadata/normal image
- loader/GPU layout
- shader contract
- actual affected shader/build target

### Step 2: Run full Aeron CI

Do not call RED5 Aeron complete until the repository's normal CI is green, not just the focused workflow.

### Step 3: Record exact Aeron commit SHA

Push `xwau-004-material-overrides`. Then update the OpenXWA `aeron` submodule pointer in a dedicated parent commit only after Aeron CI is green.

Parent commit message:

```text
build: update Aeron for XWAU legacy materials
```

---

# Task 10: RED5-E — OpenXWA XWAU render translation and SSAO controls

**Files:**
- Add: `src/xwa_remaster/xwau_material_render.h`
- Add: `src/xwa_remaster/xwau_material_render.c`
- Add: `tests/xwau_material_render_test.c`
- Modify: `.github/workflows/xwau-static-material-test.yml`

### Step 1: Write the failing translation test

Test a `.mat`-loaded material with constructor baseline + authored overrides and a synthetic `SSAO.cfg`.

Require:

```text
Glossiness 0.02 * glossiness 128.0 -> exponent 2.56
Intensity 0.25 * specular_intensity 0.5 -> 0.125
Metallic -> color-control unchanged
SpecularVal -> unchanged
Ambient -> unchanged
NMIntensity -> normalScale unchanged
lightness_boost -> copied
saturation_boost -> copied
Shadeless -> boolean
```

Also test:
- authored values >1 are not clamped;
- missing keys use shipped fallback values;
- unknown `SSAO.cfg` keys ignored;
- malformed/non-finite supported keys are explicit compatibility errors;
- no `.mat` means no legacy override at all;
- fields absent from a generic Aeron partial override are not invented inside Aeron.

### Step 2: Verify RED

Expected failure: `xwau_material_render.*` does not exist.

### Step 3: Commit RED

```bash
git add tests/xwau_material_render_test.c .github/workflows/xwau-static-material-test.yml
git commit -m "test: add XWAU material render RED contract"
```

---

# Task 11: GREEN5-E — OpenXWA owns XWAU defaults/config translation

**Files:**
- Add/modify files from Task 10.

### Step 1: Implement a small renderer-neutral config type

```c
typedef struct XwaXwauMaterialRenderConfig {
    float specular_intensity;
    float glossiness;
    float lightness_boost;
    float saturation_boost;
} XwaXwauMaterialRenderConfig;
```

Provide:
- initializer with shipped reference fallback values;
- bounded text parser for only those four keys;
- asset-root loader for `SSAO.cfg`;
- unknown keys/comments ignored;
- finite supported values required.

### Step 2: Implement a renderer-neutral resolved legacy core

Do not expose Aeron types from the parser module. `xwau_material_render` may provide an adapter function that fills an `AeronOptMaterialOverride`, but the semantic calculations remain in its own testable structures/functions.

Seed the reference constructor defaults only after a `.mat` is successfully loaded. Overlay resolved `[Default]`/named authored values, then compute effective exponent/intensity with the global config.

### Step 3: Verify GREEN and commit

```bash
git add src/xwa_remaster/xwau_material_render.h \
        src/xwa_remaster/xwau_material_render.c \
        tests/xwau_material_render_test.c \
        .github/workflows/xwau-static-material-test.yml
git commit -m "feat: translate XWAU static shading controls"
```

---

# Task 12: RED5-F — DAT-backed normal-map resolver

**Files:**
- Add: `src/xwa_remaster/xwau_normal_map.h`
- Add: `src/xwa_remaster/xwau_normal_map.c`
- Add: `tests/xwau_normal_map_test.c`
- Modify workflow.

### Step 1: Write the failing parser/load test

Use an in-test minimal DAT record encoded through the same format already covered by `xwau_dat_extended_test.c`.

Require:
- `Effects\AssaultGunboat.dat-0-7` parses to path `Effects\AssaultGunboat.dat`, group `0`, sprite ID `7`;
- parse from the two rightmost `-` separators after `.dat`, so filenames are not accidentally truncated;
- path match is case-insensitive through existing VFS behavior, not by duplicating a second file-search system;
- VFS reads the referenced DAT from asset root;
- `Xwa2d_DatAppendGroup()` decodes the requested group;
- resolver selects frame by `frame.sprite_id == requested_id`, not dense array index;
- RGBA, width and height are returned;
- malformed reference, missing DAT, missing group or missing sprite are explicit compatibility errors when the `.mat` authored that NormalMap.

### Step 2: Verify RED and commit

```bash
git add src/xwa_remaster/xwau_normal_map.h \
        tests/xwau_normal_map_test.c \
        .github/workflows/xwau-static-material-test.yml
git commit -m "test: add XWAU normal-map RED contract"
```

Do not add the production `.c` in the RED commit.

---

# Task 13: GREEN5-F — Reuse the existing DAT codec

**Files:**
- Add: `src/xwa_remaster/xwau_normal_map.c`
- Modify: `CMakeLists.txt`
- Modify workflow if link inputs are needed.

### Step 1: Implement only the adapter

- Parse the normal reference.
- `AeronVfs_ReadAll(..., AERON_VFS_ROOT_ASSET, dat_path, ...)`.
- `Xwa2d_DatAppendGroup()` into `Xwa2dFrameSet`.
- Find matching `sprite_id`.
- Transfer/copy the selected frame into a small owned image result.
- Free the frame set.
- No new DAT decompressor or texture decoder.

### Step 2: Verify GREEN

Run:
- extended DAT regression;
- HD DAT resolution regression;
- new normal-map resolver test.

### Step 3: Commit

```bash
git add src/xwa_remaster/xwau_normal_map.c \
        src/xwa_remaster/xwau_normal_map.h \
        tests/xwau_normal_map_test.c CMakeLists.txt \
        .github/workflows/xwau-static-material-test.yml
git commit -m "feat: resolve XWAU DAT normal maps"
```

---

# Task 14: RED5-G — OpenXWA OPT integration contract

**Files:**
- Add: `tests/xwau_opt_material_integration_test.py`
- Modify workflow.

### Step 1: Write source-boundary RED

Require `src/xwa_remaster/opt_mesh.c` to:

1. Load `Materials/<basename>.mat` with the existing `XwaXwauMaterial_LoadAsset`.
2. If missing, call Aeron exactly as before with no legacy overrides.
3. If loaded:
   - load XWAU render config;
   - build one generic default override (`texture_name == NULL`);
   - collect unique section names case-insensitively;
   - call `XwaXwauMaterial_Resolve()` for each unique name;
   - build one named override per unique resolved material name;
   - resolve each authored NormalMap through `xwau_normal_map`;
   - pass all override pointers/count through `AeronOptModelBuildOptions`.
4. Keep all normal-image pixels alive until synchronous `Aeron_OptModelBuildMemory()` returns.
5. Free material parse tree, override arrays, names and normal images on every success/error exit.
6. Do not set RED4 PBR `metallic_factor`/`roughness_factor` from XWAU Metallic/Glossiness.

### Step 2: Verify RED and commit

```bash
git add tests/xwau_opt_material_integration_test.py \
        .github/workflows/xwau-static-material-test.yml
git commit -m "test: add XWAU OPT material integration RED contract"
```

---

# Task 15: GREEN5-G — Wire static XWAU materials into current OPT build

**Files:**
- Modify: `src/xwa_remaster/opt_mesh.c`
- Modify: `CMakeLists.txt` if not already wired.

### Step 1: Build overrides without a second material database

Use only the lifetime of one `XwaRemasterOptMesh_Build()` call.

The default override applies constructor+`[Default]` resolved legacy values to every OPT texture. Named overrides apply fully resolved values for matching section names, using the generic Aeron default+specific merge.

Deduplicate named section labels ASCII-case-insensitively before creating Aeron records so Aeron's duplicate-specific guard remains meaningful.

### Step 2: Resolve normal images before the Aeron call

For each override with an authored `NormalMap`, resolve and own RGBA pixels. Set `NORMAL_IMAGE` and `normal_scale` in the same generic override.

### Step 3: Preserve missing-material behavior

If the `.mat` file is missing, do not load `SSAO.cfg`, do not produce a default legacy override, and keep the previous PBR/material behavior exactly.

### Step 4: Verify GREEN

Focused OpenXWA XWAU-004 workflow:
- parser 394-compatible contract;
- asset lookup;
- render config translation;
- normal-map DAT adapter;
- OPT integration source contract;
- existing XWAU-001/002/003 focused regressions where touched.

### Step 5: Commit

```bash
git add src/xwa_remaster/opt_mesh.c CMakeLists.txt \
        tests/xwau_opt_material_integration_test.py
git commit -m "feat: apply XWAU static materials to OPT models"
```

---

# Task 16: Real resource validation before calling RED5 green

Use the supplied physical project resources first.

### Step 1: Materials corpus

Run the existing corpus validator across all 394 `.mat` files. Expected: `394/394`.

Additionally enumerate every authored `NormalMap` and validate:
- reference syntax;
- referenced DAT exists in the supplied XWAU/Effects resource set when that set is expected to contain it;
- group/sprite decode succeeds.

Do not mark a missing resource in a partial uploaded ZIP as an engine failure until checked against the user's full installed XWAU tree.

### Step 2: Installed-game validation

On the user's real XWAU installation:
- recursively validate every `Materials/*.mat`;
- resolve every static NormalMap against the installed DAT resources;
- build the affected OPTs through the OpenXWA→Aeron path;
- record totals, failures and exact resource names.

### Step 3: Runtime visual probes

Select representative materials covering:
- low/high Glossiness;
- Intensity;
- Metallic below/around/above 1.1;
- SpecularVal;
- Ambient;
- Shadeless;
- NMIntensity 0, 0.5, 1 and >1 if present in corpus;
- DAT NormalMap present.

Compare against XWAU reference behavior, focusing on the material response, not identical post-process output.

`NoBloom` mismatches are expected and must be logged as the next explicit frontier, not “fixed” inside RED5.

---

# Task 17: Completion gate

Before any merge to `xwau-support`:

1. Aeron focused XWAU-004 workflow green.
2. **Full Aeron CI green.**
3. OpenXWA focused XWAU-004 workflow green.
4. **Full OpenXWA CI green.**
5. 394/394 parser corpus green.
6. Installed-game `.mat` + NormalMap resource validation green or every failure classified with evidence.
7. Real runtime probes demonstrate the intended legacy material path.
8. `NoBloom` and `AlphaIsntGlass` remain explicitly open, not silently approximated.
9. No temporary README/test artifacts accidentally staged.
10. User explicitly approves merge.

Only then prepare the XWAU-004 merge/checkpoint. Do not auto-merge.

## Self-review checklist

Before executing Task 1:
- Scan this plan for unresolved placeholder markers or implementation gaps: none are allowed.
- Confirm OpenXWA parent HEAD is the approved spec commit `d7a3915`.
- Confirm Aeron submodule resolves to GREEN4 `b8f26b8186f23a710aaae292bd0359eaee234c3b`.
- Confirm Aeron worktree is clean before RED5 test edits.
- Confirm OpenXWA temporary `README_XWAU004_*` files remain untracked and excluded.
- Re-read the approved spec and physical-source semantic notes before changing shader math.
