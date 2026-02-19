# 3D Upgrade Northstar

## Purpose
This directory tracks the full historical path of the 2D to 3D program upgrade, phase by phase, with clear deliverables and completion checkpoints.

## Workflow
1. Maintain one master plan in this file.
2. Maintain one active phase doc in `docs/3d-upgrade/phases/`.
3. Complete deliverable checkboxes for that phase.
4. When a phase is complete, move its doc into `docs/3d-upgrade/completed/`.
5. Update this file to mark that phase complete.

## Directory Layout
- `docs/3d-upgrade/README.md`: master northstar, sequencing, acceptance criteria, decision log.
- `docs/3d-upgrade/phases/phase-01.md` ... `phase-09.md`: active phase plans.
- `docs/3d-upgrade/completed/`: completed phase docs.

## Global Completion Criteria For Any Phase
A phase is complete only when all are true:
- [ ] All phase deliverable checkboxes are marked complete.
- [ ] Required code changes are implemented.
- [ ] Project compiles successfully.
- [ ] `make test` passes.
- [ ] Phase doc reflects final implemented state.

## Program Northstar
Build a 3D-capable geometry editor where the world model is fully 3D, editing is plane-constrained by default, and users can inspect geometry via free camera movement while still using a precise 2D plane workflow for creation.

## Phase Roadmap

### Phase 01: 3D Math And Core Abstraction Foundation
- [x] Introduce 3D math primitives and projection/plane helpers without changing visible behavior.
- [x] Add adapter layer so existing 2D systems continue working during migration.
- [x] Add tests for new math primitives and compatibility behavior.
Ship outcome: codebase can represent 3D math internally while still running current 2D editor behavior.

### Phase 02: 3D Data Model Migration
- [x] Promote anchor/world position storage from 2D to 3D.
- [x] Update layout/editor/core interfaces to use 3D-ready positions.
- [x] Remove or redesign 2-connection constraint as needed for 3D graph growth.
Ship outcome: geometry core stores and manipulates 3D positions safely.

### Phase 03: JSON Schema Upgrade And Backward Compatibility
- [x] Introduce new schema version with `x/y/z` anchors and required metadata.
- [x] Keep backward compatibility for existing 2D files (`z = 0` default path).
- [x] Keep undo/redo snapshot stability through schema transition.
Ship outcome: old files still load, new files preserve 3D state.

### Phase 04: Plane-Constrained Editing Pipeline
- [x] Add editable active plane modes (`XY@z`, `YZ@x`, `XZ@y`).
- [x] Map screen interaction to world using plane-aware intersection logic.
- [x] Keep point creation constrained to current active plane.
Ship outcome: users can edit cleanly on chosen slices of 3D space.

### Phase 05: Plane-Aware Rendering Cues
- [x] Render on-plane points/edges as primary visual targets.
- [x] Render off-plane points/edges as muted but selectable.
- [x] Add visual depth cue rules tied to distance from active plane.
Ship outcome: users can read depth context while preserving plane-edit precision.

### Phase 06: Selection And Hit-Test Rewrite For 3D Context
- [x] Replace purely 2D hitbox assumptions with projection-aware picking.
- [x] Rank selection candidates by screen proximity and depth rules.
- [x] Preserve stable selection behavior for points, edges, and handles.
Ship outcome: selection remains predictable in mixed depth scenes.

### Phase 07: Free Camera Inspection Mode
- [x] Add camera state for orbit/pan/zoom style 3D inspection.
- [x] Keep editing constrained by active plane while in inspect workflow.
- [x] Add fast controls for switching plane and plane offset.
Ship outcome: users can inspect from any angle without losing constrained editing workflow.

### Phase 08: UX Parity, Tooling, And Export Alignment
- [x] Update overlays/UI to show 3D coordinates and plane context.
- [x] Reconcile curved-handle behavior with 3D constraints and plane-local editing.
- [x] Verify tooling/export paths for 3D-capable data flow.
Ship outcome: complete end-to-end editor experience matches 3D core capabilities.

### Phase 09: Hardening, Performance, And Final Validation
- [ ] Add regression tests for core 3D workflows and schema compatibility.
- [ ] Validate performance and correctness across larger scenes.
- [ ] Close migration debt and finalize documentation.
Ship outcome: stable, tested, maintainable 3D-first editor baseline.

## Stretch Goals
- [ ] Arbitrary plane editing (not only axis-aligned planes).
- [ ] Advanced 3D bezier behavior and constraints.
- [ ] Additional depth visualization modes for dense geometry scenes.

## Decision Log
Use this section to track architecture decisions and pivots.

### Entry Template
- Date: YYYY-MM-DD
- Phase:
- Decision:
- Reason:
- Impact:
- Follow-up:

Note: Any major pivot is discussed with you first, then recorded here.

### 2026-02-19
- Date: 2026-02-19
- Phase: Phase 01
- Decision: Implement Phase 1 3D math utilities locally in `src/Math` instead of pulling a shared vector/plane lib.
- Reason: Current shared libs include `core_space` 2D conversion contracts but no reusable `Vec3/ray/plane` utility module yet.
- Impact: Phase 1 remains unblocked; APIs are named/structured so they can be lifted into shared libs later.
- Follow-up: Revisit extraction to shared module after Phase 2-3 stabilize call sites.

### 2026-02-19
- Date: 2026-02-19
- Phase: Phase 02
- Decision: Store anchor positions as `Vec3` now, while keeping temporary 2D wrappers and XY render/input behavior.
- Reason: This enables immediate 3D-ready core storage and APIs without forcing schema/render camera changes ahead of Phase 3-5.
- Impact: Core geometry now supports 3D world data paths; legacy APIs still work for current tooling and UI.
- Follow-up: Phase 3 will persist `z` in JSON schema and complete serialization parity.

### 2026-02-19
- Date: 2026-02-19
- Phase: Phase 03
- Decision: Promote layout schema to v3 by adding anchor `z` persistence while keeping v2/missing-version fallback (`z = 0`).
- Reason: Complete the persistence side of the 3D storage migration without breaking historical project files.
- Impact: New saves preserve 3D coordinates; old files remain loadable without manual migration.
- Follow-up: Next phases can rely on persisted depth when adding plane-based editing and depth-aware rendering.

### 2026-02-19
- Date: 2026-02-19
- Phase: Phase 04
- Decision: Implement active-plane projection using orthographic plane-view rays plus shared projection helpers, while preserving current camera model.
- Reason: Enables immediate constrained editing on `XY/YZ/XZ` planes without waiting for free-camera implementation.
- Impact: Rendering, hitboxes, and placement now follow selected plane basis; creation is constrained to plane offset.
- Follow-up: Phase 5 adds depth cue styling for off-plane geometry; Phase 7 introduces free camera.

### 2026-02-19
- Date: 2026-02-19
- Phase: Phase 05
- Decision: Apply depth attenuation only to non-highlighted geometry states, while preserving full hit-test participation for off-plane elements.
- Reason: Keeps plane readability high without undermining selection/editing workflows.
- Impact: On-plane geometry is visually dominant; off-plane geometry remains muted but still clickable.
- Follow-up: Phase 06 will add projection/depth-aware pick ordering to reduce ambiguous overlaps.

### 2026-02-19
- Date: 2026-02-19
- Phase: Phase 06
- Decision: Replace reverse-order hitbox picking with explicit tuple ranking: type priority, plane-depth distance, cursor proximity, deterministic tie-break.
- Reason: Overlapping projected geometry needed stable and intentional picks in multi-plane editing workflows.
- Impact: Picks now prefer handles/points over walls and near-plane/near-cursor targets, reducing ambiguous overlaps.
- Follow-up: Phase 07 will layer free-camera inspection on top of this selection baseline.

### 2026-02-19
- Date: 2026-02-19
- Phase: Phase 07
- Decision: Introduce a lightweight orthographic free-view basis (yaw/pitch + target), reusing existing orthographic scale/pan behavior instead of introducing perspective.
- Reason: This unblocks immediate multi-angle inspection while keeping edit mapping deterministic and preserving plane-constrained workflows.
- Impact: Rendering, hit-testing, and screen-to-plane mapping now share camera-basis helpers in math utilities when free view is enabled.
- Follow-up: Phase 08 should expand UX cues for active mode/controls and evaluate whether perspective should be optional later.

### 2026-02-19
- Date: 2026-02-19
- Phase: Phase 08
- Decision: Keep shape export format 2D but make projection axis explicit (`XY/YZ/XZ`) in conversion paths, with UI export bound to active edit plane.
- Reason: Shared shape consumers stay unchanged while 3D editing workflows can export the currently authored slice intentionally.
- Impact: `ShapeDocument_FromLayoutProjected` now drives UI/CLI projection control, and curve-handle math is plane-local across render/pick/drag.
- Follow-up: Phase 09 should add broader regression coverage for mixed-plane curved workflows and export compatibility checks.
