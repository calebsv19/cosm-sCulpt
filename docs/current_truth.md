# Line Drawing Current Truth

Last updated: 2026-04-02

## Program Identity
- repository directory: `line_drawing/`
- public product name in README: `LineDrawing`
- primary runtime entry path today:
  - `src/main.c` (`main()` delegates through `line_drawing_app_main(...)`)
  - canonical lifecycle wrapper entry:
    - `include/line_drawing/line_drawing_app_main.h`
    - `src/app/line_drawing_app_main.c`

## Current Structure
- required scaffold lanes are present:
  - `docs/`, `src/`, `include/`, `tests/`, `build/`
- normalized support lanes present:
  - `config/` (tracked defaults/static samples)
  - `data/` (runtime lanes)
  - `tmp/` (temp/archive lane; ignored)
  - `external/` (vendored dependency lane)
- active source subsystem lanes:
  - `Core`, `Editor`, `Input`, `Layout`, `Math`, `Render`, `Tools`, `UI`, `app`

## Runtime/Verification Contract (Current)
- build:
  - `make -C line_drawing clean && make -C line_drawing`
- scaffold smoke gate:
  - `make -C line_drawing run-headless-smoke`
- visual harness build gate:
  - `make -C line_drawing visual-harness`

Stable test lane:
- `make -C line_drawing test-stable`
- current composition:
  - `test`
- scene export/compile smoke lane:
  - `make -C line_drawing scene-pipeline-smoke`

Legacy test lane:
- `make -C line_drawing test-legacy`
- current composition:
  - `test-shared-theme-font-adapter` (currently failing expectation and quarantined to legacy lane)

Desktop packaging lane:
- `make -C line_drawing package-desktop`
- `make -C line_drawing package-desktop-smoke`
- `make -C line_drawing package-desktop-self-test`
- `make -C line_drawing package-desktop-refresh`
- launcher diagnostics:
  - `/Users/<user>/Desktop/LineDrawing.app/Contents/MacOS/line-drawing-launcher --print-config`
  - `tail -n 120 ~/Library/Logs/LineDrawing/launcher.log`

## Lifecycle Wrapper Snapshot
- locked lifecycle wrapper symbols are active:
  - `line_drawing_app_bootstrap`
  - `line_drawing_app_config_load`
  - `line_drawing_app_state_seed`
  - `line_drawing_app_subsystems_init`
  - `line_drawing_runtime_start`
  - `line_drawing_app_run_loop`
  - `line_drawing_app_shutdown`
- behavior-preserving delegation path:
  - `main()` calls `line_drawing_app_main(...)`
  - previous startup/runtime body remains in `line_drawing_app_main_legacy(...)`

## Runtime Config and Generated State Snapshot
- tracked defaults live in:
  - `config/`
- mutable runtime state writes to ignored lanes:
  - `data/runtime/space_mode.txt`
  - `data/runtime/theme_preset.txt`
- runtime compatibility fallback loads from legacy paths when runtime files are absent:
  - `config/space_mode.txt`
  - `theme_preset.txt`

## 2D/3D Parity Snapshot
- canonical parity lane is complete (`LD-U0` through `LD-U6.6`) in private docs:
  - `../docs/private_program_docs/line_drawing/`
- trio scene-authoring execution lane is complete (`LD3D-0` through `LD3D-4`) in private docs:
  - `../docs/private_program_docs/line_drawing/2026-04-02_ld3d4_scene_authoring_closeout.md`
- free-view axis gizmo drag contract and `Shift+C` center crosshair toggle are active.
- canonical scene authoring export contract currently includes explicit mode intent fields:
  - root: `space_mode_intent` + `space_mode_default`
  - objects: `space_mode_intent` + `dimensional_mode`
- canonical conversion contract is explicit (no hidden conversion):
  - root: `conversion_policy=explicit_only`
  - objects: explicit `projection_policy` + `extrusion_policy`
- canonical scene ID immutability is enforced on re-export:
  - when exporting to an existing scene file, existing `scene_id` is preserved.
- scene-level canonical metadata is authorable in tooling:
  - `shape_tool --export-scene ...` supports `--scene-material-id|type`, `--scene-light-id|type`, `--scene-camera-id|type`.
- canonical export enforces pre-persistence validation:
  - option-level validation (IDs/type allow-list) and payload-level validation run before scene files are written.
- local line_drawing export->compile command path is wired:
  - `tools/scene_export_compile_pipeline.sh`
  - `make -C line_drawing scene-export-compile`
  - runtime compile path uses shared `core_scene_compile` tool boundary.

## Scaffold Migration State
- private migration plan:
  - `../docs/private_program_docs/line_drawing/2026-04-01_line_drawing_scaffold_standardization_switchover_plan.md`
- baseline freeze:
  - `../docs/private_program_docs/line_drawing/2026-04-01_ld_s0_baseline_freeze_and_mapping.md`
- current phase snapshot:
  - `LD-S0` complete
  - `LD-S1` complete
  - `LD-S2` complete
  - `LD-S3` complete
  - `LD-S4` complete
  - `LD-S5` complete (verification/docs closeout synchronized; stable canonical-scene-export lane revalidated)

## App Packaging State
- private execution plan:
  - `../docs/private_program_docs/line_drawing/2026-04-01_line_drawing_app_packaging_execution_plan.md`
- packaging rollout status:
  - `LD-PK0` complete
  - `LD-PK1` complete
  - `LD-PK2` complete

## Connection Pass State
- baseline kickoff status:
  - `LD-CP0` mapped
  - `LD-CP1` and `LD-CP2` wrapper implementation landed in `src/app/line_drawing_app_main.c`
  - execution note:
    - `../docs/private_program_docs/line_drawing/2026-04-02_line_drawing_connection_pass_cp0_cp2_execution.md`
- verification snapshot (2026-04-02):
  - `make -C line_drawing clean && make -C line_drawing` pass
  - `make -C line_drawing run-headless-smoke` pass
  - `make -C line_drawing visual-harness` pass
  - `make -C line_drawing test-stable` pass
  - `make -C line_drawing test-legacy` retains expected quarantined shared-theme adapter failure lane
- next:
  - optional `LD-CP3+` extraction if deeper runtime/update/render ownership split is needed

## Wrapper Contract State
- cross-program wrapper initiative status:
  - `W0` complete
  - `W1` complete for `line_drawing`
  - `W2` complete for `line_drawing`
  - `W3` complete for `line_drawing`:
    - `S0` baseline freeze + verification rerun complete
    - `S1` typed runtime-loop adapter seam complete
    - `S2` typed run-loop handoff seam cutover complete
    - `S3` seam diagnostics + ownership hardening complete
    - `S4` closeout/docs/memory sync complete
- execution note:
  - `../docs/private_program_docs/line_drawing/2026-04-02_line_drawing_w1_w2_wrapper_hardening.md`
  - `../docs/private_program_docs/line_drawing/2026-04-02_line_drawing_w3_s0_s4_execution.md`
