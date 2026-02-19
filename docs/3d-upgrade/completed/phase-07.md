# Phase 07 Plan: Free Camera Inspection Mode

## Phase Objective
Add a free camera inspection mode so users can inspect geometry from arbitrary angles while keeping editing constrained to the active plane.

## Deliverables
- [x] Add free camera runtime state and mode toggle.
- [x] Add free camera controls for orientation and target movement.
- [x] Route render and hit-test projection through free camera basis when enabled.
- [x] Keep creation/editing constrained by active plane while in free view.
- [x] Verify build and test baseline remains green.

## Detailed Implementation Plan

### 1) Camera State And Mode Toggle
Implementation intent:
- Introduce free camera state (`enabled`, yaw/pitch, target).
- Add keyboard toggle between plane and free view modes.

Design constraints:
- Keep default startup behavior unchanged (`PLANE_VIEW`).

### 2) Free Camera Controls
Implementation intent:
- Add camera orientation controls (yaw/pitch).
- Add target translation controls for viewport repositioning.

Design constraints:
- Clamp pitch to avoid singular orientation.
- Keep controls additive and lightweight.

### 3) Projection Integration
Implementation intent:
- Add shared projection helpers that map world positions into current view basis.
- Use same projection path for rendering and hitbox rebuild.

Design constraints:
- Keep behavior deterministic across systems.

### 4) Plane-Constrained Editing In Free View
Implementation intent:
- Continue resolving placement drags/clicks onto active plane via ray/plane intersection from free camera.
- Keep axis/offset plane model from earlier phases.

Design constraints:
- Do not introduce free-camera unconstrained point creation in this phase.

### 5) Validation And Exit Checks
Required validation before marking complete:
- `make` succeeds.
- `make test` passes.
- Free-view rendering and hit-tests align on same projected geometry.
- Placement remains constrained to active plane in both view modes.

## Out Of Scope For Phase 07
- Perspective projection model.
- Occlusion-aware hidden surface picking.
- Advanced camera presets or cinematic controls.

## Risks And Notes
- Risk: render and hitbox projection drift if not sharing helpers.
Mitigation: use centralized projection and mapping helpers in math utilities.

- Risk: keybinding overlap with existing editing controls.
Mitigation: consume free camera control keys before edit keybind paths.

## Completion Record
Mark and update when phase is done.

- Date completed: 2026-02-19
- Build status: `make test` build pipeline completed successfully.
- Test status: `make test` passed (`Layout` 15 tests, `Math` 14 tests).
- Notes:
  - Added free camera state and controls with `V` mode toggle.
  - Render/hitbox/input now support free-view projection paths.
  - Editing remains active-plane constrained under both view modes.
