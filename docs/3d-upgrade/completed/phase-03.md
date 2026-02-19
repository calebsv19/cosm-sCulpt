# Phase 03 Plan: JSON Schema Upgrade And Backward Compatibility

## Phase Objective
Upgrade layout persistence to a 3D-aware schema that stores `x/y/z` anchor positions while keeping full compatibility with existing 2D schema files and preserving undo/redo snapshot stability.

## Deliverables
- [x] Bump layout JSON schema to v3 and persist `z` for anchors.
- [x] Keep backward compatibility for v2 and legacy JSON files (`z` defaults to `0`).
- [x] Keep undo/redo behavior stable with the new schema snapshots.
- [x] Update persistence docs to reflect schema v3 behavior.
- [x] Verify build and test baseline remains green.

## Detailed Implementation Plan

### 1) Schema Version Upgrade
Implementation intent:
- Update `LAYOUT_JSON_SCHEMA_VERSION` to 3.
- Continue writing existing `file` metadata fields while adding 3D anchor persistence.

Design constraints:
- Preserve existing future-version rejection behavior.
- Avoid introducing new required top-level fields beyond current save shape.

### 2) 3D Anchor Persistence
Implementation intent:
- Save `x`, `y`, and `z` for anchors.
- Keep all current anchor bezier/persistence/type fields intact.

Design constraints:
- Existing save/load semantics for non-position fields must remain unchanged.
- Schema should remain human-editable and straightforward.

### 3) Backward Compatibility
Implementation intent:
- On load, accept v3 anchors with `z`.
- On v2 or missing-version files, read `x/y` and default `z = 0`.
- Keep current wall index rebuild logic and safety checks.

Design constraints:
- No migration step required from users.
- Existing config files should load without modification.

### 4) Undo/Redo Snapshot Stability
Implementation intent:
- Keep `Layout_SaveToString`/`Layout_LoadFromString` behavior stable under schema v3.
- Verify editor history still restores layout correctly with 3D positions.

Design constraints:
- No behavior regressions in history stack operations.

### 5) Validation And Exit Checks
Required validation before marking complete:
- `make` succeeds.
- `make test` passes.
- v3 save + load roundtrip preserves `z`.
- v2/legacy loads default to `z = 0` and remain functional.

## Out Of Scope For Phase 03
- Active plane UI and plane selection controls.
- Free camera rendering.
- 3D depth-aware picking/ranking.

## Risks And Notes
- Risk: schema changes can break existing saves if fallback paths are incomplete.
Mitigation: explicit tests for v2, v3, and missing-version JSON.

- Risk: mixed historical files may contain partial fields.
Mitigation: defensive parsing defaults and strict structure checks.

## Completion Record
Mark and update when phase is done.

- Date completed: 2026-02-19
- Build status: `make test` build pipeline completed successfully.
- Test status: `make test` passed (`Layout` 14 tests, `Math` 8 tests).
- Notes:
  - JSON schema bumped to v3 (`LAYOUT_JSON_SCHEMA_VERSION = 3`).
  - Anchors now persist `z` on save.
  - Loader supports v3 `x/y/z` and v2 or missing-version files with `z = 0` fallback.
  - Added layout tests for v3 z roundtrip and v2 z default compatibility.
