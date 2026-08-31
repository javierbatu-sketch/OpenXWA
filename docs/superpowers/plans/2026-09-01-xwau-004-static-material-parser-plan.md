# XWAU-004 Static Material Parser Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:test-driven-development for every behavior change.

**Goal:** Parse and resolve the static XWA Upgrade `Materials/*.mat` syntax without translating authored values into renderer-specific PBR semantics yet.

**Architecture:** OpenXWA owns the `.mat` syntax and produces a renderer-neutral resolved material value set. `[Default]` establishes inherited values; later matching sections override only the properties they author. Dynamic material directives remain ignored in this parser phase and no Aeron API is changed here.

**Tech Stack:** C99, existing OpenXWA source tree, focused GitHub Actions C test.

**Spec:** `docs/superpowers/specs/2026-09-01-xwau-004-static-materials-design.md`

## Global Constraints

- Do not guess PBR mappings for XWAU values; real corpus values can exceed 1.0.
- Do not parse XWAU `.mat` inside Aeron.
- Do not introduce dynamic material, damage, IEVT, Dynamic Cockpit, or Active Cockpit behavior in this phase.
- Missing `.mat` integration is outside this parser-only phase.
- Parser values are preserved as authored finite floats, not clamped.
- Matching of section/material names is ASCII case-insensitive.
- Unknown directives are ignored so dynamic XWAU lines do not break static parsing.
- Supported static keys with malformed values fail explicitly.

---

### Task 1: Default inheritance and grouped section overrides

**Files:**
- Create: `src/xwa_remaster/xwau_material.h`
- Create: `src/xwa_remaster/xwau_material.c`
- Modify: `tests/xwau_static_material_test.c`

**Interfaces:**
- Produces: `XwaXwauMaterial_ParseText`, `XwaXwauMaterial_Resolve`, `XwaXwauMaterial_Free`
- Produces renderer-neutral fields for Glossiness, Intensity, Metallic, NMIntensity, Ambient, NormalMap, NoBloom and Shadeless.

- [x] **Step 1: Write the failing test**
  The committed `tests/xwau_static_material_test.c` exercises `[Default]`, a comma-separated TEX section, inheritance, numeric override and boolean flags.

- [x] **Step 2: Verify RED**
  GitHub Actions run `33451270195` failed at compile time because `xwa_remaster/xwau_material.h` and `.c` did not exist.

- [ ] **Step 3: Implement the minimal parser/resolver**
  Add the two production files. Preserve numeric values exactly as authored and overlay only `has_*` properties.

- [ ] **Step 4: Verify GREEN**
  Run the focused workflow and require a warning-free compile plus:
  `PASS: XWAU static material Default inheritance + TEX override`

- [ ] **Step 5: Commit**
  Commit only parser/resolver, updated test and this plan.

### Task 2: Real-corpus static edge cases

**Files:**
- Modify: `tests/xwau_static_material_test.c`
- Modify: `src/xwa_remaster/xwau_material.h`
- Modify: `src/xwa_remaster/xwau_material.c`

**Interfaces:**
- Extends the same API with tested support for `SpecularVal` and `AlphaIsntGlass`.

- [ ] **Step 1: Add RED tests**
  Add cases for case-insensitive section matching, later-section precedence, values above 1.0, `SpecularVal`, `AlphaIsntGlass`, and malformed supported values.

- [ ] **Step 2: Verify RED**
  Require at least one new assertion to fail for each newly introduced supported property/validation rule.

- [ ] **Step 3: Implement minimally**
  Add only behavior required by the new tests.

- [ ] **Step 4: Verify GREEN**
  Focused test must pass warning-free.

- [ ] **Step 5: Commit**
  Commit parser edge-case support.

### Task 3: Whole supplied `.mat` corpus parse validation

**Files:**
- Create or extend focused validation tooling under `tests/` without shipping corpus files in the repository.
- No renderer/Aeron production changes.

**Interfaces:**
- Consumes the same parser API.

- [ ] **Step 1: Add a validator path that accepts an external Materials directory.**
- [ ] **Step 2: Run it against all 394 supplied `.mat` files.**
- [ ] **Step 3: Classify every parser failure as supported-static syntax, intentionally ignored dynamic syntax, or malformed resource.**
- [ ] **Step 4: Add regression tests for any supported-static syntax variant discovered.**
- [ ] **Step 5: Require 394/394 static parse success before parser phase closure.**

### Phase boundary

After Task 3 is green, stop parser work. The next XWAU-004 sub-plan will cover resource lookup and renderer integration. Before that sub-plan maps Glossiness/Intensity/Metallic/NMIntensity/SpecularVal/Ambient into Aeron shading, their exact XWAU renderer semantics must be demonstrated from the reference implementation rather than inferred from generic PBR conventions.
