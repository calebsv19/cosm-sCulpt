# Line Drawing Current Truth

Last updated: 2026-04-11

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
- `make -C line_drawing release-contract`
- `make -C line_drawing release-bundle-audit`
- `make -C line_drawing release-sign APPLE_SIGN_IDENTITY="Developer ID Application: <Name> (<TEAMID>)"`
- `make -C line_drawing release-notarize APPLE_SIGN_IDENTITY="Developer ID Application: <Name> (<TEAMID>)" APPLE_NOTARY_PROFILE="<profile>"`
- `make -C line_drawing release-staple`
- `make -C line_drawing release-verify-notarized APPLE_SIGN_IDENTITY="Developer ID Application: <Name> (<TEAMID>)"`
- launcher diagnostics:
  - `/Users/<user>/Desktop/sCulpt.app/Contents/MacOS/line-drawing-launcher --print-config`
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
  - `data/runtime/input_root.txt`
  - `data/runtime/output_root.txt`
  - `data/runtime/layout_root.txt`
- runtime compatibility fallback loads from legacy paths when runtime files are absent:
  - `config/space_mode.txt`
  - `theme_preset.txt`
- startup root hygiene behavior:
  - validates configured root lanes (`input_root`, `output_root`, `layout_root`) on startup
  - falls back to defaults when missing/invalid:
    - input -> `config`
    - output -> `export`
    - layout -> `config`
  - persists corrected runtime root values immediately
  - emits explicit startup diagnostics when fallback is applied
- current config default path behavior:
  - `currentConfigPath` derives from `input_root/layout_config.json` (with legacy fallback)

## 2D/3D Parity Snapshot
- canonical parity lane is complete (`LD-U0` through `LD-U6.6`) in private docs:
  - `../../docs/private_program_docs/line_drawing/`
- trio scene-authoring execution lane is complete (`LD3D-0` through `LD3D-4`) in private docs:
  - `../../docs/private_program_docs/line_drawing/2026-04-02_ld3d4_scene_authoring_closeout.md`
- deep 3D behavior rollout execution plan is complete through `LD3D-F8` in private docs:
  - `../../docs/private_program_docs/line_drawing/2026-04-10_line_drawing_ld3d_foundation_behavior_execution_plan.md`
  - `../../docs/private_program_docs/line_drawing/2026-04-11_line_drawing_ld3d_f8_closeout.md`
- pane-refactor stabilization checkpoint and resume gate are complete through `LD3D-UIR-P6`:
  - shared-hinted/kerning-enabled panel font load path is active.
  - shared font resolution now defaults to the `ide` preset and uses the resolved `UI_REGULAR` role base point size directly for UI text.
  - Vulkan UI text now uses nearest texture sampling plus drawable-density glyph rasterization to reduce high-DPI blur in packaged/runtime builds.
  - side-pane width minima now follow current content-driven chrome targets instead of old fixed threshold jumps.
  - left/right control lanes and compact rows now center within the solved pane rects.
  - operator manual validation is recorded in the private closeout lane, and the resumed `LD3D-F8` lane is now closed on the pane-scoped baseline.
- free-view axis gizmo drag contract and `Shift+C` center crosshair toggle are active.
- scene-3D foundation contracts now include:
  - explicit scene bounds state + clamp helpers (`scene3d.bounds`)
  - explicit construction-plane state (`scene3d.constructionPlane`) with axis-aligned mode and custom-frame contract, routed through `space_mode_adapter` with legacy `XY/YZ/XZ + offset` compatibility.
  - pane-scoped construction-plane UI controls are now active for the axis-aligned authoring path:
    - right-pane `Construction` group exposes `XY` / `YZ` / `XZ` axis buttons
    - offset nudges (`-` / `+`) move the active plane by one grid step
    - typed offset dialog (`Edit x/y/z`) applies in the current display units
  - typed 3D object registry contract (`LayoutObjectStore`) with stable IDs, tombstone delete semantics, and `core_object` dimensional-mode enforcement.
  - first primitive authoring path (`LD3D-F4`) for bounded plane primitives:
    - creation contract via `Layout_CreatePlanePrimitive(...)`
    - persisted plane payloads (`width/height/frame/lock flags`) in layout schema v7 `objects3d`
    - viewport selection feedback via object render + object hitboxes (`HITBOX_OBJECT3D`)
    - keyboard tooling lane: `Shift+P` create plane (`Alt+Shift+P` creates with bounds-lock override)
    - explicit UI tooling lane: right-panel `Add Plane` button routes through the same creation contract
    - free-view scene context lane: scene-bounds wireframe renders as 3D reference geometry
  - plane primitive edit path (`LD3D-F4.5`) is now active:
    - dedicated resize handles (4 corners + 4 edges) with typed hitboxes
    - layout resize API (`Layout_ResizePlanePrimitiveFromHandle(...)`) enforces:
      - corner drag => dual-axis resize with opposite corner anchored
      - edge drag => single-axis resize with opposite edge anchored
      - minimum-size clamp + scene-bounds lock policy
    - mouse drag routing updates selected plane dimensions directly from handle drags
    - selected/hovered resize handle state is visible in viewport markers + info overlay
  - rectangular prism data-contract lane (`LD3D-F5.1`) is now active:
    - `Object3D` includes typed `RectPrismPrimitive3D` payload (`width/height/depth/frame/lock flags`)
    - object validation enforces prism min-size + frame validity contracts
    - layout schema v8 persists/loads `rectPrism` payloads deterministically in `objects3d`
  - rectangular prism creation lane (`LD3D-F5.2`) is now active:
    - layout create contract is explicit via `Layout_CreateRectPrismPrimitive(...)` + `RectPrismPrimitiveCreateParams`
    - create controls are live in authoring UX:
      - right-panel `Add Prism`
      - keyboard `Shift+R` (`Alt+Shift+R` bounds-lock override)
    - creation uses construction-plane frame with scene-bounds clamp/reject policy when bounds lock is enabled
    - prism wireframe render lane is enabled for immediate visual placement feedback during creation
    - info overlay reports prism kind and dimensions (`W/H/D`) with lock flags
  - rectangular prism selection/edit lane (`LD3D-F5.3`) is now active:
    - prism viewport hitboxes are emitted for selection parity with plane objects
    - selected prism emits resize-handle hitboxes (corner/edge classes) on the active face
    - direct viewport handle drags now resize prism local `W/H` (depth preserved) with the same one-snapshot undo policy as plane resize
    - `Alt` smooth-resize override is shared with plane/prism object-resize drags
    - free-view constrained prism-handle gizmo lane is active:
      - selecting a prism resize handle in `SPACE_MODE_3D + FREE_VIEW` emits local-axis gizmo hitboxes (`HITBOX_OBJECT3D_GIZMO_AXIS`)
      - corner handle exposes local `U/V/N` axis drags; edge handle exposes 2-axis drags (in-plane normal + `N`)
      - depth edits from face handles route through `Layout_ResizeRectPrismDepthFromFaceHandle(...)` (min-size + bounds-lock enforced)
- canonical scene authoring export contract currently includes explicit mode intent fields:
  - root: `space_mode_intent` + `space_mode_default`
  - objects: `space_mode_intent` + `dimensional_mode`
- canonical conversion contract is explicit (no hidden conversion):
  - root: `conversion_policy=explicit_only`
  - objects: explicit `projection_policy` + `extrusion_policy`
- canonical export now carries authored 3D scene settings additively under `extensions.line_drawing.scene3d`:
  - bounds: `enabled`, `clamp_on_edit`, `min`, `max`
  - construction plane: `mode`, `axis`, `offset`, `custom_frame`
- authored plane/prism primitives now export as canonical `objects[]` entries:
  - stable object IDs lift from the layout object store (`obj3d_<id>`)
  - object-local primitive dimensions/frame remain namespaced under `extensions.line_drawing.primitive_payload`
- root 2D/3D intent now reflects authored 3D state from scene settings/object-store content, so pure-primitive scenes export as `3d` without requiring nonzero anchor `z`.
- right-pane selected-object transform controls are now active:
  - `Edit Pos` opens a typed vector dialog in the current display units and applies via `Layout_SetObject3DPosition(...)`
  - `Rot X`, `Rot Y`, `Rot Z` open typed degree dialogs and apply via `Layout_RotateObject3D(...)`
  - object-context summary shows current dimensions, position, and rotation for the selected plane/prism
- the deep 3D foundation lane is complete through `LD3D-F8`; further 3D authoring expansion should begin as a new post-foundation planning lane.
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
  - `../../docs/private_program_docs/line_drawing/2026-04-01_line_drawing_scaffold_standardization_switchover_plan.md`
- baseline freeze:
  - `../../docs/private_program_docs/line_drawing/2026-04-01_ld_s0_baseline_freeze_and_mapping.md`
- current phase snapshot:
  - `LD-S0` complete
  - `LD-S1` complete
  - `LD-S2` complete
  - `LD-S3` complete
  - `LD-S4` complete
  - `LD-S5` complete (verification/docs closeout synchronized; stable canonical-scene-export lane revalidated)

## App Packaging State
- private execution plan:
  - `../../docs/private_program_docs/line_drawing/2026-04-01_line_drawing_app_packaging_execution_plan.md`
- packaging rollout status:
  - `LD-PK0` complete
  - `LD-PK1` complete
  - `LD-PK2` complete
  - release-readiness lane:
    - `RL0` complete (release contract + bundle id/product/version surface)
    - `RL1` complete (runtime Vulkan portability contract + ad-hoc local signature hardening)
    - `RL2` complete (Developer ID signing + notarize + staple + verify-notarized passed)
    - `RL3` complete (desktop runtime-lane launch/log sanity)
    - `RL4` complete (Gatekeeper + codesign + notary evidence capture)
    - `RL5` complete (docs/memory sync and release-closeout commit)

## Connection Pass State
- baseline kickoff status:
  - `LD-CP0` mapped
  - `LD-CP1` and `LD-CP2` wrapper implementation landed in `src/app/line_drawing_app_main.c`
  - execution note:
    - `../../docs/private_program_docs/line_drawing/2026-04-02_line_drawing_connection_pass_cp0_cp2_execution.md`
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
  - `../../docs/private_program_docs/line_drawing/2026-04-02_line_drawing_w1_w2_wrapper_hardening.md`
  - `../../docs/private_program_docs/line_drawing/2026-04-02_line_drawing_w3_s0_s4_execution.md`

## IR1 Input Routing State
- private execution note:
  - `../../docs/private_program_docs/line_drawing/2026-04-03_line_drawing_ir1_s0_s1_execution.md`
  - `../../docs/private_program_docs/line_drawing/2026-04-03_line_drawing_ir1_s2_policy_hardening.md`
  - `../../docs/private_program_docs/line_drawing/2026-04-03_line_drawing_ir1_s3_closeout.md`
- `IR1-S0` complete:
  - baseline top-level input map captured at `src/main.c` event callback seam.
- `IR1-S1` complete:
  - explicit `InputIntake -> InputNormalize -> InputRoute -> InputInvalidate` phase seam landed in `src/main.c`.
  - typed top-level input contracts landed:
    - `LineDrawingInputEventRaw`
    - `LineDrawingInputEventNormalized`
    - `LineDrawingInputRouteResult`
  - optional route diagnostics gate landed:
    - env: `LINE_DRAWING_INPUT_DIAG=1`
- `IR1-S2` complete:
  - explicit text-entry/global-shortcut precedence policy extracted to:
    - `src/Input/input_routing_policy.h`
    - `src/Input/input_routing_policy.c`
  - top-level normalize phase now delegates classification to policy seam.
  - explicit policy test lane added:
    - `make -C line_drawing test-input-policy`
- `IR1-S3` complete:
  - diagnostics/tracker closeout synchronized and IR1 lane marked complete for line_drawing.
- verification snapshot (2026-04-03):
  - `make -C line_drawing clean && make -C line_drawing` pass
  - `make -C line_drawing test-input-policy` pass
  - `make -C line_drawing visual-harness` pass
  - `make -C line_drawing test-stable` pass
  - `make -C line_drawing run-headless-smoke` pass
- next:
  - `RS1` render split lane when scheduled (`line_drawing` IR1 is complete/maintain-only).

## RS1 Render Split State
- private execution note:
  - `../../docs/private_program_docs/line_drawing/2026-04-03_line_drawing_rs1_s0_s1_execution.md`
  - `../../docs/private_program_docs/line_drawing/2026-04-03_line_drawing_rs1_s2_closeout.md`
- `RS1-S0` complete:
  - top-level render ownership baseline captured (`main.c` update callback + monolithic `Render_Frame` lane).
- `RS1-S1` complete:
  - explicit render split contracts landed:
    - `LineDrawingUpdateFrame`
    - `LineDrawingRenderDeriveFrame`
  - explicit phase seams landed:
    - `Render_DeriveFrame(...)`
    - `Render_SubmitFrame(...)`
  - top-level callback lane now executes explicit derive->submit handoff.
  - optional diagnostics gate landed:
    - env: `LINE_DRAWING_RS1_DIAG=1`
- `RS1-S2` complete:
  - diagnostics/tracker closeout synchronized and RS1 lane marked complete for current line_drawing scope.
- verification snapshot (2026-04-03):
  - `make -C line_drawing clean && make -C line_drawing` pass
  - `make -C line_drawing test-input-policy` pass
  - `make -C line_drawing test-stable` pass
  - `make -C line_drawing run-headless-smoke` pass
  - `make -C line_drawing visual-harness` pass
- next:
  - `RS1` lane is maintain-only for line_drawing; optional `RS1-S3+` only if deeper extraction is needed.

## Doc Sync Pass State
- latest pass: 2026-04-10 (docs-only reconciliation)
- stale public->private path references corrected to `../../docs/private_program_docs/line_drawing/...`
- current worktree drift is build-artifact only under `build/` (no additional unstaged source/doc behavior deltas)

## Data Path Contract State
- private execution note:
  - `../../docs/private_program_docs/line_drawing/2026-04-08_line_drawing_data_path_contract_execution_plan.md`
  - `../../docs/private_program_docs/line_drawing/2026-04-09_line_drawing_data_path_contract_s5_closeout.md`
- rollout status:
  - `S0` complete (baseline + policy freeze)
  - `S1` complete (root accessor + runtime state contract)
  - `S2` complete (input/output root controls + keybinds + folder chooser)
  - `S3` complete (input-root consumption migration + legacy fallback + clipped list text)
  - `S4` complete (startup root validation/fallback persistence + diagnostics)
  - `S5` complete (verification/docs/memory closeout)
