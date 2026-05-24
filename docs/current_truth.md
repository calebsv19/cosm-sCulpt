# sCulpt Current Truth

Last updated: 2026-05-24

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
- Dense-scene object selection baseline is now improved:
  - plain object-body hover/click resolves to the nearest projected object
    origin instead of whichever overlapping object bounds happen to win the
    generic body hitbox ranking
  - direct object handles/gizmos, anchor lanes, and scene-bounds lanes still
    keep higher targeting priority than generic object-body picks
  - selected and hovered authored objects now show an explicit origin marker in
    the viewport
- The editor shell modernization lane is now complete through Phase 6:
  - the old always-open grouped-control sidebars are now routed through a
    tabbed shell
  - left editor tabs are:
    - `Scene`
    - `File`
  - right editor tabs are:
    - `View`
    - `Create`
    - `Object`
  - pane targets are now rebalanced:
    - the left side remains wider for the scene list and file controls
    - the right side is reduced more aggressively through the pane-host target
      path so the viewport keeps more room without collapsing the object tab
  - existing actions are still the same underneath, but they now sit behind
    stable dedicated shell surfaces instead of one flat always-open stack
  - the left `Scene` tab is now a real scrollable scene object list:
    - authored plane/prism objects render in stable row order
    - rows show object id, primitive kind, and a compact position summary
    - row hover and row-click selection stay in sync with editor object
      selection state
    - scene-bounds controls now live at the bottom of the same left `Scene`
      lane instead of in a separate right-side scene tab
  - the right `Object` tab now has a dedicated selected-object inspector:
    - a reserved object-context card renders at the top of the tab
    - selected object id, kind, dimensions, position, rotation, and lock state
      are visible without reading a footer summary
    - object-local edit controls remain directly underneath that inspector
  - the right `View` tab now has a dedicated live state card:
    - space mode, zoom/grid state, construction-plane context, delete mode,
      and current selection state are visible above the view controls
  - the right `Create` tab now has a dedicated live state card:
    - active plane, grid step, display-unit-aware primitive preview sizing, and
      primitive readiness are visible above the create controls
  - the left `File` tab now has a dedicated `File / Session` summary card:
    - layout, scene, input root, output root, and dirty/clean session state are
      visible above the file/root buttons
    - the file lane is beginning to read like a real session surface instead of
      a pure button stack
  - the editor-local JSON/scene picker now opens against the left `File` lane
    geometry, so scene-lane controls no longer push the picker into unstable
    placement
  - the final shell polish pass is now in:
    - pane surfaces have stronger framing instead of reading like flat
      edge-attached debug columns
    - active tabs and hovered buttons now carry clearer accent treatment
    - group sections now use clearer title chips
    - the left scene list now renders inside a framed owned surface with
      stronger selected/hover row accents
    - file, view, create, and object summary cards now use top accent bands
      plus internal divider lines
  - future editor UI work can now return to smaller usage-driven follow-ups
    instead of continuing this structural shell migration
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
- The compiler-units rollout now has an initial authoring/export seam:
  - explicit toolchain commands:
    - `make -C line_drawing toolchain-contract`
    - `make -C line_drawing dump-sema-canonical-scene-export`
    - `make -C line_drawing dump-sema-canonical-scene-export-primitives`
    - `make -C line_drawing dump-sema-scene-import`
    - `make -C line_drawing clang-build`
    - `make -C line_drawing fisics-build`
  - app/toolchain packaging contract now matches the stronger scaffold shape:
    - Clang program outputs build under `build/toolchains/clang/`
    - `fisiCs` program outputs build under `build/toolchains/fisics/`
    - host test artifacts build under `build/host/`
    - desktop packaging rebuilds and copies an explicit
      `PACKAGE_TOOLCHAIN` source binary instead of whatever app binary most
      recently touched the shared `build/` tree
    - default desktop packaging still stays on the Clang lane
  - current sema customers:
    - `src/Tools/canonical_scene_export.c`
    - `src/Tools/canonical_scene_export_primitives.c`
    - `src/Tools/scene_import.c`
  - explicit scene authoring options now include:
    - `world_scale`
    - `unit_system`
    - `conversion_policy`
  - explicit primitive/export seam lengths now include:
    - primitive width / height / depth payloads
    - framing-bounds fallback half-extents
    - framing-bounds padding and bounds expansion
  - the import seam now validates:
    - supported `unit_system`
    - supported `conversion_policy`
    - numeric finite positive `world_scale`

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
    - activating a nearby browse suggestion now pivots directly into the corresponding `Scenes` or `Layouts` catalog for that root instead of leaving the user in a reordered browse suggestion list
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
  - includes `tests/test_layout_scene_export.c` option coverage for explicit
      authoring metadata
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
- The structural editor shell lane is now complete:
  - preserve the left `Scene` + `File` context lanes and right-side state-card
    tabs as the default editor shell
  - future editor-facing work should enter as smaller usage-driven polish or
    capability slices on top of that shell
- Next major downstream boundary remains consumer-side integration (first in `physics_sim`, then broader trio consumers).

## History and Deep Lane References
- Full phase ledgers and archived slices are in:
  - `/Users/calebsv/Desktop/CodeWork/docs/private_program_docs/line_drawing/`
- This file is the compressed public current-state contract.
