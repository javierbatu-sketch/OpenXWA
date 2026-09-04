# XWAU-004 Static Materials Design

## Goal

Add faithful support for the static subset of X-Wing Alliance Upgrade `Materials/*.mat` files when OpenXWA converts original/XWAU OPT models into the current Aeron render model.

## Ownership

OpenXWA owns XWA/XWAU resource semantics. Aeron remains the renderer and receives resolved render material values. Aeron must not parse XWAU `.mat` files. No legacy DMJC `dynamic_map` API or parallel material state is introduced.

## Scope

XWAU-004 covers static material properties and inheritance. The supplied corpus demonstrates at least these static properties:

- `Glossiness`
- `Intensity`
- `Metallic`
- `NMIntensity`
- `Ambient`
- `SpecularVal`
- `NormalMap`
- `NoBloom`
- `Shadeless`
- `AlphaIsntGlass`

`[Default]` establishes inherited values and named or comma-separated material sections override only the values they author. Real files may split a long section header across multiple physical lines.

Animation/event directives (`anim_once`, `anim_seq`, `frame_once`, `frame_seq`, `frame_rand`, `lightmap_seq`, `lightmap_rand`, IEVT/EVT damage state, random placement/scaling) are outside XWAU-004 static behavior and are reserved for a later stage.

Dynamic Cockpit `.dc` and Active Cockpit `.ac` are also outside this stage.

## Important semantic constraint

Do not infer generic PBR mappings from the property names.

The real supplied material corpus contains authored values above 1.0 for several properties, including Glossiness, Intensity, Metallic and NMIntensity. OpenXWA therefore preserves authored XWAU values exactly. Translation into Aeron is permitted only where the reference renderer behavior has been demonstrated.

The shortcut `roughness = 1 - Glossiness` is explicitly rejected. XWAU `Metallic` is also not the glTF/Aeron `metallic_factor`.

## Parser/resolver

OpenXWA parses the `.mat` file into renderer-neutral static values.

- `[Default]` is inherited by every material.
- A matching material section overlays only the properties present in that section.
- Section-name matching is ASCII case-insensitive.
- Comma-separated names share one override.
- Long section-name lists may continue on following lines until `]`.
- Unknown/dynamic directives are ignored by the static parser.
- A malformed value for a supported static property is an explicit compatibility error.
- Numeric values are finite floats and are not clamped by the parser.

## Resource resolution

For an OPT basename such as `AssaultGunboat`, OpenXWA probes `Materials/AssaultGunboat.mat` from the XWA asset root. Missing `.mat` preserves existing OpenXWA/Aeron material behavior. A present but invalid `.mat` is a compatibility error.

`NormalMap = Effects\Name.dat-G-F` references an XWA DAT resource. Its image-resolution integration reuses the existing XWA DAT codec path; no separate DAT decoder is introduced.

## Reference-verified XWAU material semantics

Inspection of the supplied XWA Upgrade `ddraw_effects.dll`, including its embedded DXBC shaders, establishes the following renderer behavior for the static shading core:

- `Glossiness` remains a legacy gloss/specular control. It multiplies the global glossiness term and contributes to the specular exponent/control. It is not a normalized PBR roughness value.
- `Intensity` feeds the shader specular intensity (`fSpecInt`).
- `Metallic` is preserved as an independent legacy specular-color/saturation control. The reference path stores half the authored value and multiplies by two when consumed, recovering the authored value. It is not the glTF metallic factor.
- `NMIntensity` controls the strength of the authored normal map by blending the original/geometric normal toward the sampled normal-map result.
- `SpecularVal` contributes independently to the specular value/color response.
- `Ambient` is a per-material lighting term that pushes the material response toward fully lit; it is not ambient occlusion.
- `Shadeless` bypasses normal lighting for that material.
- `NormalMap` binds the authored DAT-backed normal-map resource and uses the exact resolved `NMIntensity`.
- `NoBloom` is an explicit material policy in the reference renderer. It disables bloom contribution/engine-glow behavior for that material; it cannot be modeled faithfully by merely zeroing emissive in Aeron.
- `AlphaIsntGlass` remains unresolved at the renderer-integration level and must not be guessed.

The reference material structure also keeps `SpecularMapIntensity` as a distinct field. It is not silently folded into another static property.

## Existing Aeron boundary

Aeron currently resolves one material per fragment from its existing `AeronGltfMaterial`/mesh-owned GPU material storage. The renderer already carries base-color, normal, metallic-roughness and emissive channels through the current material path.

The XWAU implementation must extend this same path. It must not add a second renderer, a parallel material database, or XWAU parsing inside Aeron.

The existing generic `AeronOptMaterialOverride` metallic/roughness transport introduced during GREEN4 remains a valid generic Aeron feature, but XWAU static material semantics must not use it as a shortcut for `Metallic` or `Glossiness`.

## RED 5 / legacy shading core design

RED 5 covers the reference-verified static shading core as one coherent contract:

- legacy glossiness
- legacy specular intensity
- legacy metallic/specular-color control
- legacy specular value
- material ambient term
- normal-map strength
- shadeless/unlit flag
- normal-map presence/resource flow through Aeron's existing normal channel

OpenXWA resolves the XWAU `.mat` values and translates them to renderer-generic legacy material fields. Aeron exposes those fields without any XWAU names or parsing logic.

The data flow is:

`Materials/<OPT>.mat` -> OpenXWA parser/resolver -> per-texture resolved legacy material override -> Aeron OPT conversion -> `AeronGltfMaterial` -> existing mesh material GPU buffer -> existing mesh shader.

Materials without a legacy override remain on the current PBR path unchanged. A material with a legacy override selects the legacy shading behavior per material inside the existing shader path.

Aeron may grow the existing GPU material record to carry the required values. Any size/layout change must be mirrored explicitly between C and HLSL and guarded by compile-time/layout tests.

`NMIntensity` reuses the existing normal atlas/sample path and changes only the normal-map contribution strength for the legacy material. It does not create a second normal-map subsystem.

`Shadeless` is represented as a generic per-material unlit/shadeless behavior in the existing shader. It must bypass the lighting terms required by the demonstrated XWAU behavior while retaining the material's base texture/color semantics.

The legacy specular branch keeps Glossiness, Intensity, Metallic and SpecularVal as separate authored controls. It must not convert them into PBR metallic/roughness factors.

## RED 5 non-goals

RED 5 intentionally does not approximate or silently implement:

- `NoBloom`: Aeron's bloom is currently extracted from final HDR brightness, so faithful per-material exclusion needs a separate post/render policy design.
- `AlphaIsntGlass`: the exact XWAU alpha/glass decision remains to be demonstrated.
- `SpecularMapIntensity` and specular-map resource integration unless its complete source-to-shader behavior is demonstrated before that RED begins.
- dynamic/event material directives.
- Dynamic Cockpit or Active Cockpit behavior.

These remain explicit later XWAU-004/frontier tasks rather than hidden fallbacks.

## RED 5 tests

The RED 5 contract must fail before production implementation exists and must cover at least:

1. Public generic Aeron material API fields for the legacy shading core.
2. OPT material matching by real texture name and generated `TexNN` alias, preserving the existing case-insensitive/ambiguity rules.
3. Partial overrides: fields not authored by OpenXWA leave the existing material defaults/path untouched.
4. Exact preservation of finite authored values, including values above 1.0 where XWAU permits them; no PBR clamping is applied merely because the field is named Metallic or Glossiness.
5. Propagation through OPT conversion and the glTF/cooker material graph into `AeronGltfMaterial`.
6. CPU-to-GPU material-buffer layout propagation.
7. Shader contract proving a per-material legacy branch consumes separate glossiness, specular intensity, metallic legacy control, specular value, ambient, normal strength and shadeless state.
8. Regression contract proving ordinary non-XWAU/PBR materials retain the current path and existing material behavior.
9. OpenXWA integration contract proving resolved `.mat` values are supplied to Aeron for the matching OPT texture and missing `.mat` leaves the current path unchanged.

## Error handling

Malformed supported `.mat` values remain OpenXWA compatibility errors. Missing `.mat` is not an error. Aeron rejects invalid generic legacy material payloads only for structural/non-finite data that would make the renderer contract invalid; it does not reinterpret XWAU ranges.

Ambiguous material override matches remain explicit conversion errors rather than selecting an arbitrary texture.

## Validation

The supplied `Materials.zip` contains 394 `.mat` files. Parser-level validation must continue accepting all 394 without interpreting dynamic directives as static errors.

RED 5 is complete only after focused RED/GREEN tests, Aeron/OpenXWA regression CI and real resource/runtime validation demonstrate the material values reaching the intended rendered surfaces.

`NoBloom` and `AlphaIsntGlass` are not considered solved by RED 5 and remain explicit follow-up work.

No merge into `xwau-support` occurs until the complete XWAU-004 stage satisfies focused tests, full CI and real XWAU resource/runtime validation.
