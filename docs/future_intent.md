# Line Drawing Future Intent

Last updated: 2026-04-01

## Scaffold Alignment Intent
1. Keep existing 2D/3D parity behavior unchanged while normalizing scaffold contracts.
2. Standardize docs + verification + lifecycle stage wrappers to match other migrated programs.
3. Keep runtime mutable state in ignored lanes (`data/runtime/`) with fallback compatibility.
4. Preserve private/public docs separation policy.

## Planned Structural Intent
- `LD-S0` (completed):
  - baseline freeze captured (dirty working tree acknowledged and mapped before scaffold edits)

- `LD-S1` (completed):
  - added public scaffold docs:
    - `docs/current_truth.md`
    - `docs/future_intent.md`
    - `docs/README.md`

- `LD-S2` (completed):
  - added scaffold verification aliases:
    - `run-headless-smoke`
    - `visual-harness`
    - `test-stable`
    - `test-legacy`

- `LD-S3` (completed):
  - added canonical wrapper entry API:
    - `include/line_drawing/line_drawing_app_main.h`
    - `src/app/line_drawing_app_main.c`
  - locked lifecycle stage symbols and routed `main()` through `line_drawing_app_main(...)`

- `LD-S4` (completed):
  - locked runtime/temp ignore policy in `.gitignore` (`tmp/`, `data/runtime/`, `data/snapshots/`, `ide_files/`)
  - moved runtime persistence targets to `data/runtime/*` with fallback compatibility from legacy paths

- `LD-S5` (pending):
  - run full verification sweep and sync closeout docs:
    - private plan + private index
    - `line_drawing/docs/current_truth.md`
    - global scaffold matrix/backlog trackers
  - closeout commit after user confirmation with exact title:
    - `Project Scaffold Standardization`

## Desktop Packaging Intent
- packaging baseline is complete (`LD-PK0` through `LD-PK2`) with standardized `package-desktop*` targets and launcher diagnostics.
- near-term follow-up:
  - apply canonical product-name rename for packaged app:
    - `LineDrawing.app` -> `sketCh.app`
  - keep rename as a separate cleanup pass from functional packaging closeout.

## Non-Goals During Scaffold Migration
- no new editor features
- no 2D/3D behavior redesign
- no broad header relocation sweep in this pass (kept as later follow-up lane)
