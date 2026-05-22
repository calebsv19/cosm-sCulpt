# sCulpt Current Truth

Last updated: 2026-05-22

## Program Identity
- Repository directory: `line_drawing/`
- Public product name: `sCulpt`
- Internal/repo/runtime identifiers still use `line_drawing` and `LineDrawing`
  in launcher, log, binary, and source-level contracts where required
- Primary runtime entry:
  - `src/main.c` -> `line_drawing_app_main(...)`
  - wrapper shell: `include/line_drawing/line_drawing_app_main.h`, `src/app/line_drawing_app_main.c`

## Current Shipped State
- Default launch is now menu-first:
  - `src/main.c` owns a host-level `MENU` vs `EDITOR` runtime split
  - the top-level host menu is implemented under `src/Menu/`
  - the editor/runtime session remains the existing authoring workspace behind that host seam
  - the current Phase 2 slice gives the host menu explicit navigation, filter, content, detail, and footer regions instead of a flat action card
  - catalog rows and the detail pane now render lightweight cached wireframe previews plus preview-derived primitive and bounds metadata
  - the host menu now also has a dedicated browse section with header-level root-picker controls plus nearby scene-like directory suggestions around the current input root
  - the host menu now also has a dedicated recents section for reopening recent layouts/scenes and switching back to recent input/output roots
  - `Esc` in the editor now returns to the host menu instead of closing the program directly; `Esc` from the host menu still quits
  - the host shell now uses stronger pane framing, section-count badges, selected-state accent bars, and better separated preview/detail blocks so the top-level surface reads more like a durable tool shell
  - the host list lane now renders a visible draggable scrollbar for recents, layouts, scenes, and browse when content exceeds the viewport, and the top-level host also accepts `Ctrl/Cmd + B` for the native input-root picker (`Shift` variant for output root)
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

## Recommended Agent Workflow
- For new room/object scene creation, treat `line_drawing` as the upstream
  authoring source and `ray_tracing` as the downstream inspection/render lane.
- The intended loop is:
  - author or revise a request JSON
  - optionally run `line_drawing/tools/agent_scene_refine.py` to normalize
    camera placement, camera aim, prism spacing, and light-path clearance
  - compile/export through `agent_scene_tool`
  - inspect with RayTracing headless preview/material-preview lanes
  - feed approved material/light/camera changes back into the source request
- Default authoring guidance for current downstream compatibility:
  - normal scene objects should not be authored as emissive unless explicitly
    intended
  - transparent glass-like objects should use the transparent RayTracing preset
    (`material_id = 5`) in `extensions.ray_tracing.authoring.object_materials`
  - layered surface treatment should be expressed through
    `material_texture_stack` when object-level grime/oil/fog differentiation is
    needed instead of adding more one-off flat fields

## Structure
- Required lanes: `docs/`, `src/`, `include/`, `tests/`, `build/`
- Support lanes: `config/`, `data/`, `tmp/`, `external/`
- Active source subsystems:
  - `Core`, `Editor`, `Input`, `Layout`, `Math`, `Menu`, `Render`, `Tools`, `UI`, `app`

## Runtime Contract
- Default runtime ingress is the host menu, not the editor:
  - the host currently exposes five content sections:
    - `Quick Actions`
    - `Recents`
    - `Layouts`
    - `Scenes`
    - `Browse`
  - quick actions still expose resume, reopen-last-layout, reopen-last-scene, and quit
  - reopen-last-layout and reopen-last-scene now remember independent last-opened targets, so opening a scene does not replace the last JSON/layout reopen target and opening a JSON/layout does not erase the last scene reopen target
  - recents now mix recent layouts, scenes, input roots, and output roots into one activation lane
  - layouts/scenes now browse the current input root through an app-local catalog backend
  - layouts/scenes now support inline name/path filtering from the host surface itself
  - layout/scene rows now show lightweight metadata summaries plus compact projected wireframe thumbnails
  - the detail pane now renders a larger cached preview and preview-derived counts/extents for the selected layout or scene entry
  - the browse section now focuses on root context instead of general filesystem traversal:
    - explicit native picker buttons for input root and output root in the browse header
    - ranked nearby child/sibling/cousin directory suggestions that switch the input root toward likely scene-storage locations
    - nearby browse rows now resolve representative scene/layout preview targets so the list can show the same lightweight wireframe thumbnails used elsewhere in the host shell
  - mouse hover is now visual-only; committed section and row selection changes only on explicit click or keyboard movement
  - the list scrollbar now supports wheel scroll, track click, and thumb drag instead of relying on wheel-only movement
  - `Esc` now returns from the editor to the host menu when transient text-entry and authoring overlays are inactive
  - `Ctrl/Cmd + M` still returns from the editor to the host menu as an explicit chord
  - the in-editor JSON/scene picker remains an editor-local quick picker, not the top-level catalog surface
  - previews are app-local deterministic menu previews, not full editor-camera or downstream render thumbnails
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
- Current local drift now includes a durable app-host upgrade:
  - menu-first host split before the editor session
  - scene-directory export under output root
  - immediate runtime compile handoff
  - stable test coverage for scene export paths
- The next bounded local expansion is Phase 2 of the host lane:
  - deeper catalog ergonomics on top of the current browse/filter/preview shell
  - any remaining menu visual work should now be a smaller follow-up polish lane rather than another structural host-shell slice
- Next major downstream boundary remains consumer-side integration (first in `physics_sim`, then broader trio consumers).

## History and Deep Lane References
- Full phase ledgers and archived slices are in:
  - `/Users/calebsv/Desktop/CodeWork/docs/private_program_docs/line_drawing/`
- This file is the compressed public current-state contract.
