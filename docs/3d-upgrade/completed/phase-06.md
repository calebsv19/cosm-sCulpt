# Phase 06 Plan: Selection And Hit-Test Rewrite For 3D Context

## Phase Objective
Replace last-hitbox-wins behavior with projection-aware ranked picking that remains stable in overlapping depth scenarios and prioritizes the most relevant target near the cursor.

## Deliverables
- [x] Add ranked hit evaluation for points, walls, and handles.
- [x] Incorporate active-plane depth distance into pick ranking.
- [x] Incorporate cursor proximity into pick tie-breaking.
- [x] Preserve intuitive type priority (handles/points over walls).
- [x] Verify build and test baseline remains green.

## Detailed Implementation Plan

### 1) Ranked Pick Model
Implementation intent:
- Evaluate all hitboxes under cursor and choose best candidate by score tuple:
  - type priority,
  - depth distance from active plane,
  - cursor distance to hitbox center,
  - deterministic final tie-break.

Design constraints:
- Keep logic deterministic.
- Avoid regressions in basic single-target scenarios.

### 2) Depth-Aware Tie Breaking
Implementation intent:
- Store depth metadata per hitbox at rebuild time.
- Prefer nearer-to-active-plane candidate when projected overlap occurs.

Design constraints:
- Keep off-plane geometry selectable.
- No camera-depth sort yet; use active-plane distance only in this phase.

### 3) Cursor Proximity Tie Breaking
Implementation intent:
- Include center-distance check so smaller/more local target wins within overlapping class.

Design constraints:
- Do not require expensive geometry checks beyond current hitbox model.
- Keep per-event pick cost low.

### 4) Priority And Compatibility
Implementation intent:
- Preserve expected hierarchy: handles > points > walls > none.
- Keep existing input workflows unchanged except for improved overlap resolution.

Design constraints:
- Do not alter selection-state APIs in this phase.
- Keep event routing and editor behavior stable.

### 5) Validation And Exit Checks
Required validation before marking complete:
- `make` succeeds.
- `make test` passes.
- Overlapping projected points select the better-ranked candidate.
- Existing non-overlap selection flows remain stable.

## Out Of Scope For Phase 06
- Free camera depth picking.
- Occlusion culling.
- Arbitrary geometry raycasts beyond current hitbox model.

## Risks And Notes
- Risk: ranking weights can feel inconsistent in edge cases.
Mitigation: use explicit tuple ordering instead of opaque weighted sums.

- Risk: priority changes might alter established click habits.
Mitigation: keep handles/points-over-walls hierarchy unchanged.

## Completion Record
Mark and update when phase is done.

- Date completed: 2026-02-19
- Build status: `make test` build pipeline completed successfully.
- Test status: `make test` passed (`Layout` 15 tests, `Math` 12 tests).
- Notes:
  - Replaced reverse-order hitbox return logic with ranked candidate selection.
  - Ranking tuple now uses type priority, active-plane depth distance, cursor proximity, then deterministic insertion tie-break.
  - Updated hitbox rebuild metadata to track depth distance using full active plane (`axis + offset`).
  - Added overlap regression coverage to ensure near-plane point selection wins when projections coincide.
