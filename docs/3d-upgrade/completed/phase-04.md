# Phase 04 Plan: Plane-Constrained Editing Pipeline

## Phase Objective
Add active plane editing modes (`XY@z`, `YZ@x`, `XZ@y`) and route screen interactions through plane-aware world mapping so point creation is constrained to the selected plane.

## Deliverables
- [x] Add active edit plane state and controls (`XY`, `YZ`, `XZ`, offset).
- [x] Map screen interactions to world points through plane-aware intersection logic.
- [x] Constrain point creation/wall placement to the current active plane.
- [x] Keep current 2D viewport workflow stable while projecting by active plane basis.
- [x] Verify build and test baseline remains green.

## Detailed Implementation Plan

### 1) Active Plane State And Controls
Implementation intent:
- Add persistent active plane state (`axis`, `offset`) to global/editor-accessible runtime state.
- Add keyboard controls to switch plane and adjust plane offset.

Design constraints:
- Default to `XY@z=0` so current behavior remains familiar on startup.
- Keep controls lightweight and additive.

### 2) Screen To Plane World Mapping
Implementation intent:
- Add helper path that maps screen points into active-plane world points.
- Use plane-aware math (ray/plane intersection) and keep snapping behavior compatible.

Design constraints:
- Preserve existing snap semantics.
- Handle non-intersection/degenerate cases safely.

### 3) Plane-Constrained Creation
Implementation intent:
- Update placement path so new points/walls are created on active plane.
- Keep axis-lock behavior for two-click wall creation, but evaluate locking in plane-local axes.

Design constraints:
- No free camera yet; this phase remains orthographic plane-view oriented.

### 4) Plane Projection Compatibility Across View Systems
Implementation intent:
- Update render/hitbox/select paths to project `Vec3` through current active plane basis instead of hardcoded XY.
- Keep UI/overlay visibility coherent with the selected plane mode.

Design constraints:
- Do not introduce depth styling yet (Phase 5).
- Keep existing UX stable for users staying on `XY@0`.

### 5) Validation And Exit Checks
Required validation before marking complete:
- `make` succeeds.
- `make test` passes.
- Plane switch + offset produces expected constrained placement behavior.
- Existing XY workflow remains stable.

## Out Of Scope For Phase 04
- Free camera inspect mode.
- Depth-muted rendering cues for off-plane geometry.
- Projection-aware depth-ranked picking heuristics.

## Risks And Notes
- Risk: projection inconsistencies across render and hit-testing can break selection.
Mitigation: centralize plane projection helpers and reuse across systems.

- Risk: changing plane basis may surprise users if not visible.
Mitigation: expose active plane state in overlay text.

## Completion Record
Mark and update when phase is done.

- Date completed: 2026-02-19
- Build status: `make test` build pipeline completed successfully.
- Test status: `make test` passed (`Layout` 14 tests, `Math` 10 tests).
- Notes:
  - Added active plane state (`XY/YZ/XZ + offset`) to global runtime state.
  - Added keyboard controls: `1/2/3` for plane select and `[`/`]` for plane offset (shift = 10x step).
  - Added centralized screen-to-plane mapping via ray/plane math helpers in `math_util.h`.
  - Updated render, hitboxes, ghost-wall preview, selection-in-box, and creation flow to use active plane projection instead of fixed XY.
  - Updated overlay status line to display active plane and offset.
