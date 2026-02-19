# Phase 02 Plan: 3D Data Model Migration

## Phase Objective
Promote core geometry storage and interfaces from 2D-only world positions to 3D-ready world positions, while preserving current 2D editing/render behavior as the compatibility path.

## Deliverables
- [x] Migrate anchor/world position storage to 3D (`Vec3`) in layout data structures.
- [x] Provide 3D-ready layout/editor interfaces for anchor and wall creation/mutation.
- [x] Redesign connection constraints so 3D graph growth is possible (remove global 2-connection cap).
- [x] Keep current runtime behavior stable through temporary 2D compatibility adapters.
- [x] Verify build and test baseline remains green.

## Detailed Implementation Plan

### 1) Layout Storage Upgrade To 3D
Implementation intent:
- Promote `Anchor.pos` from `Vec2` to `Vec3`.
- Keep existing rendering/input behavior by using XY projection (`Vec2_FromVec3`) where needed.
- Ensure all layout operations (add/remove/compact/drag/etc.) operate on 3D position storage.

Design constraints:
- No functional rendering camera change in this phase.
- Existing scene interaction should remain visually 2D.

### 2) 3D-Ready API Surface
Implementation intent:
- Add 3D-native APIs for creation/edit flows, such as:
  - `Layout_AddAnchor3(...)`
  - `Layout_AddWall3(...)`
- Keep compatibility wrappers for current 2D call sites during migration.
- Start migrating editor placement state to 3D-aware world positions.

Design constraints:
- Avoid broad breakage by preserving current call paths where possible.
- Keep naming consistent and explicit between 2D compatibility and 3D-native entry points.

### 3) Connection Constraint Redesign
Implementation intent:
- Remove hard global `connectionCount < 2` cap that blocks general 3D graph growth.
- Preserve curve-anchor constraints so bezier behavior remains valid (`curve` anchors still require/maintain 2-connected semantics).

Design constraints:
- Prevent invalid handle topology for curved anchors.
- Allow corner anchors to scale beyond 2 connections.

### 4) Compatibility Path Stabilization
Implementation intent:
- Update hitbox/render/input and utility conversions to consume `Vec3` storage safely by flattening to XY for now.
- Maintain JSON schema behavior from Phase 1/legacy path (schema upgrade is Phase 3).

Design constraints:
- No schema bump in this phase.
- Preserve undo/redo and existing project load/save behavior.

### 5) Validation And Exit Checks
Required validation before marking complete:
- `make` succeeds.
- `make test` passes.
- Existing 2D interaction flow is not regressed in expected use.
- Phase document reflects implemented behavior and known carryovers to Phase 3.

## Out Of Scope For Phase 02
- JSON schema v3 with `z` persistence.
- Active plane UI and plane-constrained editing controls.
- Free camera and 3D projection rendering mode.
- 3D-aware picking depth ranking.

## Risks And Notes
- Risk: temporary 2D wrappers can hide incomplete 3D adoption.
Mitigation: explicitly mark 3D-native entry points and migrate call sites incrementally.

- Risk: curve-handle assumptions may conflict with high-degree graph nodes.
Mitigation: keep curve anchor semantics constrained while allowing non-curve anchors to exceed 2 connections.

## Completion Record
Mark and update when phase is done.

- Date completed: 2026-02-19
- Build status: `make test` build pipeline completed successfully.
- Test status: `make test` passed (`Layout` 12 tests, `Math` 8 tests).
- Notes:
  - `Anchor.pos` migrated to `Vec3` storage in layout core.
  - Added 3D-native creation APIs: `Layout_AddAnchor3` and `Layout_AddWall3`.
  - Kept compatibility wrappers (`Layout_AddAnchor`, `Layout_AddWall`) for existing 2D call sites.
  - Removed hard 2-connection cap for corner anchors; curve anchors remain constrained to preserve bezier semantics.
  - JSON remains schema v2 with XY persistence in this phase; `z` serialization is deferred to Phase 3.
