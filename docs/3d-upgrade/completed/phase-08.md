# Phase 08 Plan: UX Parity, Tooling, And Export Alignment

## Phase Objective
Complete user-facing 3D parity by ensuring plane context is clearly surfaced, curved-handle editing remains plane-local and stable in mixed view modes, and export/tooling paths align with 3D workflows.

## Deliverables
- [x] Confirm overlays/UI expose 3D and plane context consistently.
- [x] Reconcile curved-handle math with plane-local constraints across render, hit-test, and drag editing.
- [x] Align Layout->Shape tooling/export with explicit projection-plane behavior.
- [x] Add tests that lock in plane-local handle behavior and projected export behavior.
- [x] Verify build/test baseline remains green.

## Detailed Implementation Plan

### 1) Overlay/UI Parity
Implementation intent:
- Validate that selection and status overlays always include 3D coordinates and active plane context.
- Ensure control discoverability is documented in top-level keybinding reference.

### 2) Curve Handle Plane-Local Behavior
Implementation intent:
- Centralize plane-local handle conversion helpers in math utilities.
- Use shared helpers in render and hitbox pipelines so handles are projected from world consistently.
- Update handle dragging to compute angle/length from plane-local world delta (camera-independent).

Design constraints:
- Preserve existing handle data model (`length + angle`), but make interpretation explicitly plane-based.
- Keep deterministic behavior between render and pick geometry.

### 3) Tooling/Export Alignment
Implementation intent:
- Add projection-axis aware Layout->Shape conversion.
- Keep default XY conversion for backward compatibility.
- Route UI export through active edit plane projection.
- Add optional CLI flag to select export plane (`xy|yz|xz`).

Design constraints:
- Do not break existing shape export consumers.
- Keep export format unchanged while making flattening behavior explicit.

### 4) Tests And Exit Checks
Required validation before marking complete:
- `make` succeeds.
- `make test` passes.
- Handle drag/render/hit-test share the same plane-local conversion assumptions.
- UI export uses active plane projection and CLI can select projection plane.

## Out Of Scope For Phase 08
- Perspective projection export.
- Volumetric/mesh export formats.
- Arbitrary (non-axis-aligned) plane editing/export.

## Risks And Notes
- Risk: Handle behavior regressions in legacy XY flows.
Mitigation: Keep XY default path and add tests for helper behavior.

- Risk: Export projection changes break expectations.
Mitigation: Preserve XY default and add explicit plane flag + UI active-plane mapping.

## Completion Record
Mark and update when phase is done.

- Date completed: 2026-02-19
- Build status: `make` reports clean success; runtime/test objects compile successfully.
- Test status: `make test` passed (`Layout` 16 tests, `Math` 16 tests).
- Notes:
  - Curved-handle render, hitbox, and drag math now share plane-local world conversion helpers.
  - UI shape export now projects via active plane; CLI shape tool supports `--plane xy|yz|xz`.
  - Added regression coverage for handle polar roundtrips and projection-axis export mapping.
