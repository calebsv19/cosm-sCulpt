# Phase 01 Plan: 3D Math And Core Abstraction Foundation

## Phase Objective
Establish 3D-capable math and abstraction layers while preserving current 2D runtime behavior so migration can continue safely.

## Deliverables
- [x] Add foundational 3D math primitives and operations in the Math module.
- [x] Add projection and plane-intersection helpers for future plane-constrained editing.
- [x] Introduce compatibility adapters so current 2D input/render/editor paths compile and run unchanged.
- [x] Add tests covering new math behavior and 2D compatibility expectations.
- [x] Verify build and test baseline remains green.

## Detailed Implementation Plan

### 1) 3D Math Primitive Introduction
Implementation intent:
- Add `Vec3` and related helpers in `src/Math/math_util.h` or split into a 3D companion header if cleaner.
- Include operations needed by upcoming phases: add/sub, scalar multiply, dot, cross, length, normalize, distance.
- Keep naming consistent with existing style (`Vec2_*` pattern mirrored to `Vec3_*`).

Design constraints:
- Do not break existing `Vec2` API.
- Keep functions lightweight/inline where appropriate.
- Preserve warning-clean builds under current flags.

### 2) Plane And Projection Utilities
Implementation intent:
- Define simple plane representation for migration (`normal + distance` or `axis + offset` helpers).
- Add utility functions for:
  - ray-plane intersection,
  - point-to-plane signed distance,
  - world point projection to plane-local coordinates (for future 2D editing views).

Design constraints:
- Axis-aligned support first, structured for future arbitrary planes.
- Numeric stability checks for near-parallel ray/plane cases.

### 3) 2D Compatibility Adapter Layer
Implementation intent:
- Add conversion helpers between 2D and 3D where needed:
  - `Vec2 -> Vec3` defaulting `z = 0`,
  - `Vec3 -> Vec2` for temporary legacy paths.
- Keep existing 2D rendering/input behavior unchanged in this phase.

Design constraints:
- No visible UX change in Phase 01.
- Existing call sites should require minimal churn.

### 4) Test Coverage For Foundation Layer
Implementation intent:
- Extend `tests/test_math.c` with targeted unit tests for new 3D math operations.
- Add tests for plane intersection edge cases and signed distance behavior.
- Add compatibility tests validating round-trip assumptions used by current 2D runtime.

Design constraints:
- Tests should be deterministic and small.
- Keep test runner wiring consistent with current harness style.

### 5) Validation And Exit Checks
Required validation before marking complete:
- `make` succeeds.
- `make test` passes.
- New tests added for all newly introduced math categories.
- No behavior regressions observed in existing 2D editor flow.

## Out Of Scope For Phase 01
- Migrating layout anchor storage to 3D.
- Changing JSON schema.
- Adding active edit plane UI.
- Changing picking/hitbox systems.
- Free camera behavior.

## Risks And Notes
- Risk: introducing partial 3D types without strict adapter boundaries may create mixed-API confusion.
Mitigation: document conversion paths and keep temporary adapters explicit.

- Risk: overbuilding math APIs not needed yet.
Mitigation: only add primitives directly required by Phase 02-04 roadmap.

## Completion Record
Mark and update when phase is done.

- Date completed: 2026-02-19
- Build status: `make test` build pipeline completed successfully.
- Test status: `make test` passed (`Layout` 10 tests, `Math` 8 tests).
- Notes:
  - Added 3D math foundation in `src/Math/math_util.h` without changing runtime edit/render behavior.
  - Added Phase 1 math tests in `tests/test_math.c`.
  - Updated module docs in `src/Math/README.md`.
