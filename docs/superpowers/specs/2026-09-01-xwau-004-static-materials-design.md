# XWAU-004 Static Materials Design

## Goal

Add faithful support for the static subset of X-Wing Alliance Upgrade `Materials/*.mat` files when OpenXWA converts original/XWAU OPT models into the current Aeron render model.

## Ownership

OpenXWA owns XWA/XWAU resource semantics. Aeron remains the renderer and receives resolved render material values; Aeron must not parse XWAU `.mat` files. No legacy DMJC `dynamic_map` API or parallel material state is introduced.

## Scope

XWAU-004 covers static material properties and inheritance only:

- `[Default]` material defaults.
- Texture/material section overrides such as `[TEX00007,TEX00008]`.
- `NormalMap`.
- `NMIntensity`.
- `Metallic`.
- `Glossiness`.
- `Intensity`.
- `Shadeless`.
- `NoBloom`.
- `Ambient`.

Animation/event directives (`anim_once`, `frame_once`, `frame_seq`, `frame_rand`, `lightmap_seq`, IEVT/EVT damage state, random placement/scaling) are explicitly out of scope and reserved for the next stage.

Dynamic Cockpit `.dc` and Active Cockpit `.ac` are also out of scope.

## Resource resolution

For an OPT basename such as `AssaultGunboat`, OpenXWA probes `Materials/AssaultGunboat.mat` from the XWA asset root. Missing `.mat` is not an error and preserves the existing OpenXWA/Aeron OPT material behavior.

If a `.mat` exists, OpenXWA parses the static subset, resolves `[Default]`, then overlays every matching material/texture section. Malformed values in supported static properties are treated as XWAU compatibility errors instead of being silently ignored.

Unsupported dynamic directives are recognized as out-of-scope and ignored by XWAU-004 without changing static resolution.

## Material identity

Section names are matched to authored OPT texture/material identity, not to incidental runtime material slot order. Comma-separated section names apply the same override to every listed authored identity.

## Render mapping

OpenXWA resolves XWAU semantics into a small renderer-neutral override structure and supplies it to the existing runtime OPT conversion path. Existing alpha override handling remains independent.

Aeron is extended only where its current PBR material lacks a required static value. Existing fields are reused where semantics are direct:

- XWAU `Metallic` -> Aeron metallic factor.
- XWAU `Glossiness` -> Aeron roughness using `roughness = clamp(1 - glossiness, 0, 1)`.
- XWAU `NormalMap` -> normal channel source for the matching material.

The following require explicit current-Aeron representation rather than semantic guessing:

- `NMIntensity` normal strength.
- `Intensity` material lighting/specular intensity.
- `Shadeless` unlit lighting mode.
- `NoBloom` bloom contribution exclusion.
- `Ambient` authored ambient contribution.

These are added minimally to the current Aeron material/shader path; no second renderer or XWAU-specific renderer is created.

## Normal-map source

`NormalMap = Effects\Name.dat-G-F` references an XWA DAT resource, group and frame. OpenXWA resolves and decodes that frame through the existing XWA 2D/DAT codec support added in XWAU-002/003. The result is then supplied to the runtime OPT material cook. No separate DAT decoder is added.

## Failure semantics

- Missing `.mat`: use existing material path.
- Existing valid `.mat`: apply resolved static values.
- Existing `.mat` with malformed supported static value: fail that XWAU material load with an explicit diagnostic.
- Missing referenced `NormalMap`: explicit compatibility failure for that authored normal map; do not silently replace it with an invented texture.
- Dynamic directives: ignored in XWAU-004, to be implemented in the next stage.

## Validation

TDD starts with parser/resolver tests using real syntax from supplied XWAU material files. Tests cover default inheritance, multi-texture override, normal-map reference parsing, numeric override precedence, boolean flags and malformed supported values.

After parser GREEN, integration tests verify that resolved values reach the current Aeron material model. Full Windows/Linux/macOS CI must pass. Final validation uses real XWAU `.mat` resources and an in-game OPT comparison with and without XWAU material application.

No merge into `xwau-support` occurs until focused tests, full CI and real resource/runtime validation are green.
