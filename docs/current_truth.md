# sCulpt Current Truth

Last updated: 2026-05-21

## Program Identity
- Repository directory: `line_drawing/`
- Public product name: `sCulpt`
- Internal/repo/runtime identifiers still use `line_drawing` and `LineDrawing`
  in launcher, log, binary, and source-level contracts where required
- Primary runtime entry:
  - `src/main.c` -> `line_drawing_app_main(...)`
  - wrapper shell: `include/line_drawing/line_drawing_app_main.h`, `src/app/line_drawing_app_main.c`

## Current Shipped State
- 2D/3D parity lane is complete (`LD-U0` through `LD-U6.6`).
- Trio scene-authoring and deep 3D behavior foundation lanes are complete through `LD3D-F8`.
- Primitive authoring contract is active for planes and rectangular prisms with typed object payloads.
- Agent-authored room-review scenes now have an optional deterministic
  refinement lane through `line_drawing/tools/agent_scene_refine.py` for:
  - opposite-corner default camera placement in open corner rooms
  - authored camera-path points placed outside the floor footprint by default
  - camera-path edits no longer forced back inside authored scene bounds
    because the refiner disables `bounds.clamp_on_edit` on refined requests
  - authored camera yaw/pitch now match focus-target direction so app-side
    camera vectors and startup previews agree with headless runtime sampling
  - transparent tall-prism spacing cleanup
  - sampled RayTracing light-path clearance around object clusters
- Scene export/compile path is wired and deterministic for canonical scene contract fixtures, and the desktop UI exports full scenes as stable per-scene directories through the configured output root.
- The current scene-directory export contract is:
  - derive a scene stem from the current layout filename
  - create `<output-root>/<scene-stem>/`
  - write `scene_authoring.json`
  - compile `scene_runtime.json` immediately through shared `core_scene_compile`
  - preserve the resulting authoring/runtime paths for UI diagnostics/logging

## Structure
- Required lanes: `docs/`, `src/`, `include/`, `tests/`, `build/`
- Support lanes: `config/`, `data/`, `tmp/`, `external/`
- Active source subsystems:
  - `Core`, `Editor`, `Input`, `Layout`, `Math`, `Render`, `Tools`, `UI`, `app`

## Runtime Contract
- Runtime roots and persisted runtime-state lanes are explicit and normalized.
- Startup root hygiene/fallback behavior is active for input/output/layout roots.
- Legacy config fallback behavior remains for compatibility when runtime files are absent.
- Output-root export behavior is now user-visible and deterministic:
  - `Export Shape` still writes a single exported shape artifact
  - `Export Scene` writes a scene directory with both authoring and runtime scene files

## Verification Contract
- Build/harness:
  - `make -C line_drawing clean && make -C line_drawing`
- Stable tests:
  - `make -C line_drawing test-stable`
  - includes `tests/test_scene_export.c` in the current worktree
- Headless wording note:
  - `make -C line_drawing run-headless-smoke`
  - currently routes through `test-stable` rather than a separate runtime-only lane
- Build-only readiness:
  - `make -C line_drawing visual-harness`
- Scene pipeline smoke:
  - `make -C line_drawing scene-pipeline-smoke`
- Packaging/release lanes:
  - `make -C line_drawing package-desktop*`
  - `make -C line_drawing release-contract`
  - `make -C line_drawing release-bundle-audit`
  - `make -C line_drawing release-sign ...`
  - `make -C line_drawing release-notarize ...`
  - `make -C line_drawing release-staple`
  - `make -C line_drawing release-verify-notarized ...`

## Current Boundary
- `line_drawing` is closed as upstream authoring/export source for current primitive scope.
- Current local drift is export-UX hardening, not a new authoring-contract expansion:
  - scene-directory export under output root
  - immediate runtime compile handoff
  - stable test coverage for scene export paths
- Next major downstream boundary remains consumer-side integration (first in `physics_sim`, then broader trio consumers).

## History and Deep Lane References
- Full phase ledgers and archived slices are in:
  - `/Users/calebsv/Desktop/CodeWork/docs/private_program_docs/line_drawing/`
- This file is the compressed public current-state contract.
