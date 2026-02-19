# Phase 05 Plan: Plane-Aware Rendering Cues

## Phase Objective
Add depth-aware visual cues so geometry on the active edit plane is emphasized while off-plane geometry remains visible and selectable with muted styling.

## Deliverables
- [x] Render on-plane anchors and walls with primary emphasis.
- [x] Render off-plane anchors and walls with muted depth-based styling.
- [x] Keep off-plane geometry selectable (no hit-test exclusion in this phase).
- [x] Add configurable/clear depth cue rules tied to active plane distance.
- [x] Verify build and test baseline remains green.

## Detailed Implementation Plan

### 1) Depth Cue Rule Definition
Implementation intent:
- Define plane-distance based intensity rules for anchors, walls, and handles.
- Keep selected/hovered states visually dominant over depth dimming.

Design constraints:
- Avoid abrupt flicker around the plane boundary.
- Keep colors readable on existing grid/background.

### 2) Wall And Curve Rendering Integration
Implementation intent:
- Apply depth cue attenuation to wall/curve color and thickness decisions.
- Use endpoint distance to active plane (or midpoint approximation) for segment styling.

Design constraints:
- Preserve selected wall highlight priority.
- Keep curve readability while dimming off-plane segments.

### 3) Anchor/Handle Rendering Integration
Implementation intent:
- Apply depth cue attenuation to anchor and handle fill/line colors.
- Preserve hover/selection/drag/pinned overrides while depth cueing non-active states.

Design constraints:
- Click targets remain intact (no radius reduction tied to depth).
- Handle visibility remains sufficient for editing.

### 4) Selection/Hit Testing Policy
Implementation intent:
- Keep all anchors/walls/handles selectable regardless of depth.
- Do not introduce depth-ranked pick order yet (Phase 06).

Design constraints:
- No behavior regressions in existing click/drag flows.

### 5) Validation And Exit Checks
Required validation before marking complete:
- `make` succeeds.
- `make test` passes.
- On-plane geometry is visibly stronger than off-plane geometry.
- Off-plane geometry remains selectable/editable.

## Out Of Scope For Phase 05
- Free camera mode.
- Depth-ranked selection resolution.
- Arbitrary plane styling profiles.

## Risks And Notes
- Risk: aggressive dimming can make off-plane geometry too hard to use.
Mitigation: cap attenuation so geometry remains legible.

- Risk: selection colors may conflict with depth styling.
Mitigation: treat selection/hover/drag states as higher-priority style layers.

## Completion Record
Mark and update when phase is done.

- Date completed: 2026-02-19
- Build status: `make test` build pipeline completed successfully.
- Test status: `make test` passed (`Layout` 14 tests, `Math` 12 tests).
- Notes:
  - Added depth attenuation rule tied to active-plane distance and applied it to non-selected wall/anchor/handle rendering.
  - Kept hover/selection/drag visual states dominant over depth dimming.
  - Preserved hit-testing/selectability for off-plane geometry (no phase-5 exclusion policy).
  - Added math tests for view-plane distance helpers.
