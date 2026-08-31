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

The real supplied material corpus contains authored values above 1.0 for several properties, including Glossiness, Intensity, Metallic and NMIntensity. Therefore OpenXWA must first preserve the authored XWAU values exactly. Translation into Aeron's shading fields will be implemented only after the XWAU renderer/reference semantics for each property are demonstrated.

The previously proposed shortcut `roughness = 1 - Glossiness` is explicitly withdrawn until reference behavior proves it.

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

For an OPT basename such as `AssaultGunboat`, the integration phase will probe `Materials/AssaultGunboat.mat` from the XWA asset root. Missing `.mat` preserves existing OpenXWA/Aeron material behavior.

`NormalMap = Effects\Name.dat-G-F` references an XWA DAT resource. Its actual image-resolution integration will reuse the existing XWA DAT codec path; no separate DAT decoder will be introduced.

## Renderer integration boundary

The parser does not alter Aeron.

After parser/corpus coverage is green, a separate XWAU-004 integration sub-plan will map reference-verified XWAU semantics into the existing Aeron material/shader path. Aeron will be extended only where the current material representation genuinely lacks a required value.

## Validation

TDD begins with default inheritance and grouped section overrides. Real-corpus syntax variants become regression tests before production behavior is added.

The supplied `Materials.zip` contains 394 `.mat` files. Parser-level validation must accept all 394 without interpreting dynamic directives as static errors. Semantic coverage of every supported static property is tested separately.

No merge into `xwau-support` occurs until focused tests, full CI and real resource/runtime validation are green.
