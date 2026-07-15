# Line Drawing Future Intent

Last updated: 2026-07-14

## Scene Editor Stabilization Intent

- Treat the current Bounds/Wire/Solid/Material split, collision-free pane
  layout, contextual Create workflow, and validated property editing as the
  LineDrawing usability baseline.
- Loaded-scene lifecycle proof is complete: source identity, dirty state, Save,
  Save As, Export Scene, close, and reopen retain stable ids/bindings.
- Complex-mesh quality-state separation is complete: hover/selection and
  appearance do not demote the mesh; zoom/pan retain settled geometry; only
  viewing-direction or mesh-geometry changes use the refined interaction tier.
- Shared extraction of the proven coherent indexed LOD builder is complete in
  `core_mesh_preview` `0.5.0`; LineDrawing consumes it through a thin adapter
  while retaining renderer and interaction policy locally.
- Complete the remaining constrained-size/font and hands-on
  hover/select/drag conflict proof before calling every LineDrawing usability
  boundary stable. That proof can proceed independently of mesh-preview
  consumer adoption.
- Audit and adopt the shared mesh-preview contract in RayTracing next. Reuse
  renderer-neutral preview geometry and preserve Bounds/Wire/Solid/Material
  meaning, while retaining RayTracing-specific native depth rendering,
  shading, lighting, render camera, materials, and overlays.
- Extract additional shared live-editor behavior only after LineDrawing and a
  consumer demonstrate the same stable contract. Prefer existing core/kit
  boundaries and keep renderer policy app-local.

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

- `LD-S5` (completed):
  - run full verification sweep and sync closeout docs:
    - private plan + private index
    - `line_drawing/docs/current_truth.md`
    - global scaffold matrix/backlog trackers
  - closeout commit after user confirmation with exact title:
    - `Project Scaffold Standardization`
  - verification snapshot (2026-04-02):
    - `make -C line_drawing clean && make -C line_drawing` pass
    - `make -C line_drawing run-headless-smoke` pass
    - `make -C line_drawing visual-harness` pass
    - `make -C line_drawing test-stable` pass
    - `make -C line_drawing test-legacy` keeps expected quarantined shared-theme adapter failure lane

## Desktop Packaging Intent
- packaging baseline is complete (`LD-PK0` through `LD-PK2`) with standardized `package-desktop*` targets and launcher diagnostics.
- release-readiness lane is now active:
  - `RL0` complete:
    - release contract (`release-*` targets), canonical product/bundle/version fields
    - package output aligned to canonical app name (`sCulpt.app`)
  - `RL1` complete:
    - bundled Vulkan portability contract (`libMoltenVK.dylib`, `libvulkan.1.dylib`)
    - runtime ICD env contract (`VK_ICD_FILENAMES`, `VK_DRIVER_FILES`)
    - runtime shader root in writable runtime lane
    - ad-hoc signature hardening after local install-name mutations
  - current:
    - `RL2` complete: Developer ID signing + notarization + staple + verify-notarized
    - `RL3` complete: desktop runtime-lane launch/log sanity
    - `RL4` complete: Gatekeeper/codesign/notary evidence capture
    - `RL5` complete: docs/memory/commit closeout
  - next:
    - release-readiness lane is maintain-only for `line_drawing`; use same contract for future version bumps.

## Connection Pass Intent
- baseline complete:
  - `LD-CP0` baseline routing/ownership map captured
  - `LD-CP1` stage/context ownership lock landed in wrapper
  - `LD-CP2` explicit runtime dispatch seam landed with typed request/outcome contract
  - execution note:
    - `../../docs/private_program_docs/line_drawing/2026-04-02_line_drawing_connection_pass_cp0_cp2_execution.md`
- next:
  - optional `LD-CP3+` extraction only if deeper wrapper/runtime ownership split is needed

## Cross-Program Wrapper Initiative
- `W0` complete (canonical wrapper contract frozen)
- `W1` complete for `line_drawing` (typed wrapper context + dispatch seam alignment)
- `W2` complete for `line_drawing` (structured wrapper diagnostics and final wrapper summary logging)
- `W3` complete for `line_drawing`:
  - `S0` baseline freeze + verification rerun complete
  - `S1` typed runtime-loop adapter seam complete
  - `S2` typed run-loop handoff seam cutover complete
  - `S3` seam diagnostics + ownership hardening complete
  - `S4` closeout/docs/memory sync complete
- next:
  - optional `W4+` / `LD-CP3+` only if deeper wrapper/legacy ownership extraction is needed
- execution note:
  - `../../docs/private_program_docs/line_drawing/2026-04-02_line_drawing_w1_w2_wrapper_hardening.md`
  - `../../docs/private_program_docs/line_drawing/2026-04-02_line_drawing_w3_s0_s4_execution.md`

## IR1 Input Routing Intent
- baseline setup:
  - `IR1-S0` complete (top-level input map captured before edits)
  - `IR1-S1` complete (typed phase seam landed in `src/main.c`)
  - `IR1-S2` complete (explicit text-entry/global-shortcut precedence policy seam + policy test lane)
  - `IR1-S3` complete (diagnostics/tracker closeout)
  - execution note:
    - `../../docs/private_program_docs/line_drawing/2026-04-03_line_drawing_ir1_s0_s1_execution.md`
    - `../../docs/private_program_docs/line_drawing/2026-04-03_line_drawing_ir1_s2_policy_hardening.md`
    - `../../docs/private_program_docs/line_drawing/2026-04-03_line_drawing_ir1_s3_closeout.md`
- next slices:
  - `IR1` lane complete for `line_drawing` (maintain-only)
  - queue `RS1` render-split lane when scheduled

## RS1 Render Split Intent
- baseline setup:
  - `RS1-S0` complete (top-level render ownership map captured)
  - `RS1-S1` complete (typed update/derive/submit seam landed)
  - `RS1-S2` complete (diagnostics/tracker closeout)
  - execution note:
    - `../../docs/private_program_docs/line_drawing/2026-04-03_line_drawing_rs1_s0_s1_execution.md`
    - `../../docs/private_program_docs/line_drawing/2026-04-03_line_drawing_rs1_s2_closeout.md`
- next slices:
  - `RS1` lane complete for `line_drawing` (maintain-only)
  - optional `RS1-S3+` only if deeper extraction is needed later

## Non-Goals During Scaffold Migration
- no new editor features
- no 2D/3D behavior redesign
- no broad header relocation sweep in this pass (kept as later follow-up lane)

## Data Path Contract Intent
- `S0-S5` data-path contract lane is complete and maintain-only.
- user-facing root controls are now standard:
  - input root and output root (typed + folder chooser flows)
- startup root hygiene is now deterministic:
  - configured roots are validated on startup
  - invalid/missing roots are corrected to defaults and persisted
- future work should add behavior on top of this contract instead of replacing it ad hoc.

## Current Direction
- The scaffold, wrapper, release, and original authoring/export contract lanes
  are maintain-only.
- Broader feature expansion is now intentionally tracked by the active private
  CAD architecture lane:
  `../../docs/private_program_docs/line_drawing/active/2026-06-02_sculpt_blender_level_cad_architecture_plan.md`.
- Current local work should stay bounded to explicit workflow slices:
  - topbar/menu-bar interaction where the behavior mirrors existing backend
    commands
  - scene-directory export UX and output-root clarity
  - File-pane session/browser ergonomics
  - object center-gizmo move/rotate/size polish
  - semantic object-authoring operations when deformation or topology changes
    are required
- Avoid adding raw mesh/topology mutation as a UI shortcut. Richer CAD behavior
  should land through object-authoring operations with tests and diagnostics.
- The editor pane shell is now in a maintain/polish state rather than a
  structural refactor state:
  - the five-pane tab model is established
  - follow-up UI work should stay usage-driven inside specific panes
  - future pane work should prefer smaller workflow slices such as:
    - scene row filtering/grouping
    - file-pane quick actions
    - richer create presets / staged dimensions
    - deeper object-inspector editing
    - construction-plane topbar picker/stepper controls
    - export destination/open-folder feedback

## Host Split Intent
- `LD-HS1` complete:
  - default launch now enters a top-level host menu before the editor session
  - `src/main.c` owns the app-local `MENU` vs `EDITOR` mode boundary
  - the new menu implementation lives under `src/Menu/`
  - Phase 1 scope intentionally stopped at host routing and narrow reopen actions
- `LD-HS2` next:
  - `S1` complete:
    - replaced the flat launch card with structured navigation/content/detail/footer regions
    - added the first app-local scene catalog backend for layouts and authoring scenes under the input root
    - moved text fitting onto explicit clipped render lanes
  - `S2` complete:
    - tightened host-menu spacing and selected-state contrast for denser use of the shell
    - added inline filtering for layout/scene names and paths directly in the top-level catalog lane
  - `S3` complete:
    - added richer row metadata for layout and scene entries
    - added an app-local lightweight preview cache that loads temporary layout snapshots and renders deterministic wireframe thumbnails inline
    - upgraded the detail pane to show selected-entry counts and bounds extents derived from the preview cache
  - `S4` complete:
    - added a dedicated browse section to the host menu
    - added app-local root-context controls instead of a deep filesystem browser
    - added browse-header native picker controls for input/output roots plus nearby scene-like directory suggestions from child, sibling, and cousin branches
    - nearby browse suggestions now resolve representative scene/layout previews so root switching has more visual context
    - kept fast root switching in the host while leaving full arbitrary filesystem search to the macOS picker flow
  - `S5` complete:
    - split hover feedback from committed selection so mouse hover no longer mutates menu state
    - tightened preview/detail rendering so metadata no longer sits directly on top of the large wireframe preview
  - `S6` complete:
    - added a dedicated recents section that reopens recent layouts/scenes and switches back to recent input/output roots
    - added app-local recent-context persistence so host-menu recents survive editor loads plus root changes
    - changed editor `Esc` so it returns to the top-level menu instead of directly quitting the program
  - `S7` complete:
    - tightened the shell framing so nav, list, detail, and footer panes read as one stronger physical host surface
    - added count/kind/status badges plus selected-state accent bars for clearer section and row hierarchy
    - tightened preview/detail presentation so the large preview block, summary labels, and info rows feel less flat
  - `S8` complete:
    - split quick-action remembered reopen state so last layout/JSON and last scene stay independent
    - kept scene loads from overwriting the menu's last-layout quick action target
  - `S9` complete:
    - added a visible draggable scrollbar lane for recents, layouts, scenes, and browse
    - added top-level `Ctrl/Cmd + B` input-root picking plus `Shift + Ctrl/Cmd + B` output-root picking through the existing native folder chooser flow
  - next:
    - continue only bounded visual follow-up where real usage still exposes rough edges
    - only consider richer camera-based thumbnails after the cheap deterministic preview lane has proven useful
- boundary rule:
  - keep the editor-local load menu as a quick-switch surface
  - keep the richer top-level browser/catalog behavior in the host lane
