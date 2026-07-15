# sCulpt Current Truth

Last updated: 2026-07-14

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
- Scene-editor display is now an explicit four-mode contract:
  - `Bounds` renders only transform/bounds cages;
  - `Wire` renders the real mesh preview feature edges plus a transparent,
    view-dependent coherent-LOD outline so smooth complex meshes retain a
    readable perimeter;
  - `Solid` renders an opaque, depth-tested runtime-mesh surface through
    coherent interactive/settled LODs;
  - `Material` uses the same real surface path with app-local material tinting.
- Complex-mesh quality state is separated from projection and interaction
  feedback: zoom, pan, viewport resize, and plane-offset changes reraster the
  established LOD without demotion; orbit direction and mesh-transform changes
  may briefly use an 8,000-triangle/60%-scale tier before returning to the
  18,000-triangle/75%-scale settled tier. Appearance changes retain the current
  tier, and hover/selection draw later without recoloring or demotion.
- Shared `core_mesh_preview` `0.5.0` now owns the renderer-neutral coherent
  indexed LOD builder used by complex-mesh editor previews. LineDrawing keeps
  only its viewport quality policy, CPU depth rasterization, outline
  composition, texture cache, and interaction overlays app-local.
- Legacy saved scenes that still reference `Desktop/<mesh-library>` recover the
  known `Desktop/stls/<mesh-library>` relocation at read time. The scene file
  is not silently rewritten, and shared canonical mesh data remains unchanged.
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
  - object workspace remaps the same shell into CAD authoring terms:
    - left `Model`: object-asset status, body/sketch/operation counts, current
      selection, and first-pass body/sketch/operation rows from
      `ObjectAuthoring`; rows now select the matching body, sketch, or
      operation context, and operation history uses a clipped scroll lane with
      wheel/hover/scrollbar handling for long stacks. Its object-mode action
      contract is selection and navigation, not command launching or asset I/O
    - left `Assets`: object asset save/load/new/export actions plus asset
      browser/path controls; it owns file/root actions, not model selection
    - right `Tools`: face/sketch/extrude actions plus a static `Command
      Actions` card and a separate `Active Command` card for current target,
      sketch, selected-operation, and extrude-depth state; armed extrude
      previews now expose `Depth -` / `Depth +` controls that adjust by the
      current grid step. Its action contract is command selection,
      arm/commit, and live command-parameter editing
    - right `Properties`: selected object/entity details for operations,
      sketches, bodies, and selected faces plus dimension, gizmo, and transform
      controls; it owns selected-entity inspection and persistent body/object
      edits, while command tools stay in `Tools`
    - right `View`: viewport navigation, zoom, view-plane, and editing-mode
      controls
    - object-authoring refs now carry stable app-local face ids for the current
      primitive/evaluated face model while preserving primitive face labels as
      the UI compatibility adapter. Object-mode `Model`, `Tools`,
      `Properties`, and the top status overlay now expose those stable
      `FaceID` values beside the compatibility face labels
    - face-ref diagnostics now classify unset, missing-body, missing-face, and
      stale-adapter states. Replay/evaluation fails invalid sketch/extrude
      refs explicitly, and object-mode `Model` / `Properties` rows show the
      face-ref status
    - Phase D editable topology now has its first base: object-authoring
      documents rebuild deterministic evaluated vertex, edge, and face records
      for plane and rectangular-prism bodies; faces bind to corner vertex ids
      and boundary edge ids; edges retain bounded face adjacency; and explicit
      vertex/edge selection refs are available for constrained-gizmo movement.
      The object viewport now draws evaluated vertex/edge overlays in object
      mode, emits topology hitboxes above generic object-body picks, and
      preserves clicked/hovered vertex or edge refs in the authoring document.
      Selected topology vertices/edges now resolve into the existing object
      constrained-gizmo lane, rendering legal local axes at the selected
      vertex or edge midpoint and emitting gizmo-axis hitboxes. Topology gizmo
      drags are non-mutating until the next semantic topology-operation slice,
      because the current evaluated topology is rebuilt from primitive bodies.
      The object-mode `Model` summary reports evaluated body/vertex/edge/face
      counts
    - object center-gizmo work now has first-pass mode parity for scene
      objects: `Move`, `Rotate`, and `Size` render with distinct endpoint
      visuals, and active drags report move distance, rotate angle, or size
      factor through the topbar. `Size` scales the selected object uniformly
      from any center-gizmo axis, while `Shift` applies directional stretch
      along the grabbed axis where the selected primitive supports it. Shift
      also keeps move/rotate drags in the smooth/non-quantized path.
    - selected-object exact dimension controls now support width/height edits
      for both planes and rectangular prisms; exact depth remains
      rectangular-prism-only
    - object-authoring runtime mesh compile now has a first Phase B baseline:
      operation-backed documents evaluate into deterministic runtime mesh
      arrays, current stable face ids become `face_<face_id>` surface groups,
      bounds/triangles/group references validate before success, and saved
      operation-backed assets can write `mesh_asset_runtime_v1` JSON through an
      app-local carrier over the vendored shared
      `CoreMeshAssetRuntimeContract`
    - object-mode now exposes the first Phase B runtime export affordance:
      `Export Mesh` compiles the attached operation-backed authoring document,
      writes a `<asset>.runtime.json` runtime mesh sidecar beside the object
      asset root, and reports export diagnostics plus the last runtime mesh
      path in the object workspace summary
    - imported STL metadata now has a bounded CLI/test harness:
      `make -C line_drawing imported-mesh-harness-smoke` builds
      `src/Tools/imported_mesh_harness.c`, feeds deterministic STL fixtures
      through `third_party/codework_shared` `core_mesh_asset` +
      `core_mesh_compile`, writes `mesh_asset_authoring_v1` imported-mesh
      metadata, writes file-backed `mesh_asset_runtime_v1`, emits a one-object
      `scene_runtime_v1` with a `mesh_asset_instance`, and records an
      `import_summary.json`. The harness currently covers the ASCII
      tetrahedron baseline plus the richer stepped-column fixture
      (`16` vertices, `24` triangles) used by downstream RayTracing visual
      proof. Through vendored `core_mesh_compile` `0.5.0`, the same harness now
      supports bounded ASCII and binary STL import proofs, including direct
      user-file binary imports, while keeping UI import work deferred. This is
      not yet the in-app file picker/import UI; it is the first bounded input
      harness that ties the shared imported-mesh path into LineDrawing.
      The in-app STL file browser now uses the shared file catalog with bounded
      recursive STL discovery, so selecting a curated library root can surface
      nested paths such as `curated/<asset>/source/<asset>.stl` instead of only
      files in the selected directory or one child level.
    - Phase C scene asset instance integration now has a complete app-local
      reusable asset instance baseline:
      scene mode can place the last exported object runtime mesh sidecar as a
      transformable `mesh_asset_instance`, stores the sidecar asset id/path,
      bounds, vertex count, and triangle count in layout JSON schema v9,
      summarizes mesh instances in the Scene/Object panes, renders a bounds-box
      proxy in the viewport, and exports the object as canonical
      `geometry_ref.kind = "mesh_asset"` with LineDrawing mesh-instance
      extension metadata. Scene mode also exposes a `Mesh Assets` file-browser
      mode that scans `.runtime.json` sidecars under the object asset root
      and places the selected sidecar through that same transformable instance
      path. Switching a selected mesh asset instance into object mode now
      derives the source object asset path from `<asset>.runtime.json`,
      reloads `<asset>.json`, and attaches its authoring document when present.
      Returning to scene mode refreshes every scene mesh instance that shares
      the source runtime sidecar, preserving each scene object id and transform
      while updating asset id, bounds, vertex count, and triangle count
      metadata. Full mesh viewport rendering, material overrides, richer
      imported-mesh picker UI, and deeper asset-library management remain
      Phase D-adjacent follow-up work rather than blockers for the Phase C
      reusable-instance lifecycle.
  - pane targets are now rebalanced:
    - the left side remains wider for the scene list and file controls
    - the right side is reduced more aggressively through the pane-host target
      path so the viewport keeps more room without collapsing the object tab
  - existing actions are still the same underneath, but they now sit behind
    stable dedicated shell surfaces instead of one flat always-open stack
  - the left `Scene` tab is now a real scrollable scene object list:
    - the pane now starts with a dedicated `Scene / Selection` summary card
    - the summary shows total object count, plane/prism split, and current
      selected-object context including size and lock state
    - the summary also exposes anchor/wall counts so the pane reflects the
      broader scene graph
    - authored plane/prism objects render in stable row order
    - rows now show object id, primitive kind, position, size, and lock
      metadata in a denser three-line presentation
    - the scene list now behaves like a selection-first object browser:
      - single-click selects a row without changing the expanded layout state
      - double-click on the same row toggles it open or closed
      - expanded rows surface rotation, scale, and frame-origin context
    - row hover and row-click selection stay in sync with editor object
      selection state
    - the scene list now exposes a visible draggable scrollbar instead of
      relying on wheel-only scrolling for longer object sets
    - the scene browser now renders through an explicit clipped viewport:
      - expanded rows stay inside the list surface instead of bleeding into
        selection or bounds controls
      - the scrollbar is slimmer and separated from row content by a dedicated
        gutter instead of reading like part of the row lane
    - the pane now includes scene-local `Clear Select` and `Delete Obj`
      actions below the list
    - scene-list hit routing is clipped to the real list surface so the lower
      scene controls are not swallowed by generic list clicks
    - scene-pane layout now uses explicit owned section rects for:
      - summary
      - scene browser
      - selection actions
      - bounds controls
      instead of deriving the browser height from later button bounds
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
    - the file lane now also has an explicit owned browser section below the
      file/session-path controls instead of relying on a transient popup overlay
    - `Load JSON` and `Load Scene` now switch that browser section into
      persistent JSON/scene modes
    - the browser now stays active across normal file-pane actions instead of
      disappearing as soon as another panel action runs
    - browser content now renders with:
      - mode title
      - current input-root path
      - clipped entry list
      - active-row highlight
      - slimmer scrollbar lane
      - helper footer copy
    - clicking a JSON/scene row now loads that entry while keeping the browser
      active
    - file-browser mode now persists in ignored runtime state so the pane can
      restore whether it was last in JSON mode or scene mode
    - startup now follows that persisted file-browser mode:
      - JSON mode reopens the last loaded layout when available
      - Scene mode reopens the last loaded scene when available
    - the host-menu catalog and the editor-local file browser now share one
      app-local discovery helper for JSON layouts plus authored-scene
      directories, which narrows the earlier risk of the two surfaces drifting
      apart on what counts as a loadable scene
    - recent-context history now seeds the last-known layout/scene startup
      paths before the restore pass runs
    - after startup restore, the file browser rebuilds from the real active
      session state so the active row highlight comes from the loaded file
      rather than a UI-only remembered selection
    - the file browser now also persists a remembered last JSON entry and last
      scene entry for cases where no active loaded session currently matches
      the browser root
    - the file browser now also persists separate JSON and scene browser roots
      instead of relying on one shared browsing root for both modes
    - browser rebuilds resolve the active row by preferring:
      - the real active loaded layout/scene path for the current mode
      - otherwise the remembered last JSON/scene entry for that mode
    - remembered-entry persistence now runs through the shared layout/scene
      load path instead of only browser-row clicks, so direct loads and startup
      restore keep the browser highlight coherent
    - direct layout/scene loads now refresh the browser immediately, so row
      highlighting updates as soon as session state changes instead of waiting
      for the next browser-only interaction
    - `Load JSON` and `Load Scene` now split behavior by click depth:
      - single-click switches the lower browser mode and uses the persisted
        root for that mode
      - double-click opens the corresponding mode-specific folder picker and
        updates that mode’s saved browser root
    - plain browser-mode switching no longer rewrites the live session root
      just to repopulate the lower browser
    - actual loads from the lower browser still re-establish the live input
      root from the active browser root before loading
    - if the input root changes and neither the active loaded session nor the
      remembered entry exists under that root, the browser now degrades to no
      highlighted row rather than implying stale selection state
    - the `File / Session` summary now includes an explicit browser-status line
      so the pane tells the user whether the current browser state reflects an
      active session row, a remembered row, a mode with no matching row, or a
      mode with no entries
    - the same summary card now also renders an explicit action-hint line, and
      the browser footer now reuses that same state-aware hint text so the
      user can tell when:
        - `Use Session` is just re-centering a true active row
        - `Use Session` is restoring the live session away from a remembered
          fallback row
        - `Clear Last` is only relevant because the current row is remembered
    - the browser-state explanation now runs through one shared helper
      contract, so the summary card, browser footer, and regression tests all
      describe the same active-versus-remembered semantics
    - the browser list now also marks those two row types visually:
      - true active-session rows render with a `LIVE` chip
      - remembered fallback rows render with a distinct `LAST` chip plus a
        warmer row accent
      - unmatched roots stay chip-free, so the file pane no longer relies on
        text alone to distinguish remembered fallback from the real live row
    - the generic root controls are now explicitly labeled as `Session Paths`
      so they no longer imply that they edit the same root as the lower
      JSON/scene browser
    - `Session In Edit` / `Session In Pick` now target only the live session
      input root; mode-specific JSON/scene roots remain owned by the
      corresponding `Load JSON` / `Load Scene` double-click picker flow
    - the file summary now shows `Browse In` separately from `Session In` so
      the current mode-specific browser root is visible independently from the
      live session input root
    - session input-root edits now skip unnecessary browser rebuilds when the
      active JSON/scene browser is pinned to a different saved mode root, which
      keeps the two control lanes from fighting for ownership
    - the file pane now has a stronger structural layout:
      - the top `File / Session` card is denser and shorter
      - the persistent JSON/scene browser now owns the elastic middle section
      - `File / IO` and `Session Paths` are anchored at the bottom instead of
        consuming the browser’s height first
      - those bottom file-pane control groups now use compact 2-column button
        rows where appropriate instead of always using full-width stacked
        buttons
    - the file-tab browser no longer blocks left-pane resizing:
      - splitter drag now gets first chance before the persistent file browser
        captures a click, so the left pane can still be resized while the
        `File` tab is active
    - browser refresh now scrolls the active/remembered row into view instead
      of always resetting long lists to the top
    - `File / IO` now also exposes compact browser quick actions:
      - `Use Session` retargets the browser to the best current working-session
        row for the active mode:
        - the current loaded layout/scene path when it still exists
        - otherwise the most recent layout/scene path from recent-context
          history
      - session input-root edits now preserve that real loaded layout identity
        instead of rewriting it back to the new root's default
        `layout_config.json`, so `Use Session` can recover a true active row
        after ordinary session-root changes
      - `Clear Last` removes the remembered fallback entry for the active
        browser mode so the pane can degrade cleanly to no highlighted row
        when no real active-session match exists
    - the browser header now shows mode plus entry count instead of repeating
      the current browse root above the list; the root remains visible in the
      summary card instead
    - file summary lines, browser rows/footer, and compact file-pane button
      labels now clip to their pane/view bounds instead of forcing `...`
      substitution, so pane resizing behaves more like narrowing a viewport
    - active file-browser rows and active file/scene tab fills now use subtler
      blended fills so bright presets keep readable contrast
    - invalid or empty candidate roots are rejected before mutating the
      current input root, so a failed folder pick does not clobber the prior
      working root
    - `Export Scene` now writes scene directories under the configured output
      root and uses the active layout/scene path only as a naming hint; it no
      longer silently overwrites the active authoring directory just because a
      scene is loaded
    - successful scene export promotes the exported `scene_authoring.json` as
      the active/recent scene session, while failed exports clean up newly
      created incomplete scene directories
    - export feedback is visible but temporary: the File summary and
      `Export Scene` button briefly show success/failure state, then return to
      the normal action label
    - strict authored-versus-compiled scene truth remains explicit:
      `scene_runtime.json` is still compiled output only, and the regression
      suite now directly checks that import rejects it as a load source
    - the unattended scene-pipeline and agent-scene smoke lanes now resolve
      their tool binaries through the current Makefile path contract instead of
      assuming the older flat `build/bin/` layout
  - the editor-local JSON/scene browser is now owned by the left `File` lane
    geometry instead of floating as a transient lower overlay that disappears
    after unrelated actions
  - the live runtime frame renderer now routes through the unified
    `Render_UIPanel(...)` pass, so scene/view/create/object summary surfaces
    are actually drawn in the packaged app instead of only existing in the
    panel modules
  - the final shell polish pass is now in:
    - pane surfaces have stronger framing instead of reading like flat
      edge-attached debug columns
    - active tabs and hovered buttons now carry clearer accent treatment
    - group sections now use clearer title chips
    - the left scene list now renders inside a framed owned surface with
      stronger selected/hover row accents
    - file, view, create, and object summary cards now use top accent bands
      plus internal divider lines
  - the pane language is now more internally consistent:
    - top summary cards use denser stable copy instead of taller debug-style
      phrasing
    - middle workspace/inspector surfaces use a slightly darker shared fill so
      they read as intentional working layers instead of the same flat pane
      background
    - summary cards, list rows, browser rows, and right-pane lower controls
      now route through shared clipping/layout/visual helpers instead of
      ad hoc per-pane overflow rules
  - future editor UI work can now return to smaller usage-driven follow-ups
    instead of continuing this structural shell migration
- The editor topbar is now the production top-level menu/status lane rather
  than an overlay diagnostic string:
  - one renderer owns the top-pane surface and avoids stacked duplicate text
    overlays
  - it shows workspace mode, clipped selected-object context, file/dirty state,
    mode, view, plane, construction-plane readout, bounds state, gizmo mode,
    live operation, and undo/redo controls
  - `Mode`, `View`, `Plane`, `Bounds`, `Gizmo`, `Undo`, and `Redo` are
    clickable status chips wired to the same backend actions as their keyboard
    shortcuts
  - `CP` remains a readout until construction-plane picker/stepper behavior is
    deliberately designed
  - active center-gizmo drags update a separate `Op` chip and the primary
    selection line with move, rotate, or size operation reports
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
  - first-class RayTracing lighting intent synthesis through
    `extensions.ray_tracing.authoring.lighting_policy`
  - front-biased generated review modes now start camera-side of the subject
    by default and keep control points inside the requested front hemisphere
    unless a full orbit, rim/backlight path, or `allow_backlight` is explicit
  - supported generated-lighting intents include front key orbit, front
    corkscrew, front vertical sweep, full/fixed-height object orbit, high
    shadow orbit, rim light, and transparent-review lighting
  - `extensions.ray_tracing.authoring.ambient_policy` now expands into the
    RayTracing bridge's existing `authoring.environment` fill-light settings
    for none/review-fill/transparent-fill/studio modes
- `agent_scene_tool` now accepts first-class `mesh_asset_instance` request
  objects for RayTracing-facing mesh probes:
  - the request supplies a stable authored object id, `asset_id`,
    request-relative `asset_source_path`, transform, variant, and optional
    canonical `material_id`
  - the tool skips mesh instances during Layout primitive-store creation, then
    injects them into `scene_authoring.json` as full-3D
    `object_type = mesh_asset_instance` objects with
    `geometry_ref.kind = mesh_asset`
  - referenced runtime mesh sidecars are copied into both
    `<out>/assets/mesh_assets/` and the app-loadable
    `<run_dir>/line_drawing_app_load/assets/mesh_assets/` folder
  - the low-poly sphere fixture renders through RayTracing headless as 48 mesh
    triangles plus a 2-triangle floor, proving the request schema, canonical
    compile, asset sidecar lookup, and BVH render path are connected
  - the same schema has been used for higher-fidelity moving-light mesh-sphere
    worker proofs; the authored scene remains LineDrawing-owned while
    RayTracing owns runtime mesh loading, native `3D` triangle build, BVH
    traversal, shading, and publication
- Scene export/compile path is wired and deterministic for canonical scene contract fixtures, and the desktop UI exports full scenes as stable per-scene directories through the configured output root.
- The current scene-directory export contract is:
  - derive a scene stem from the current layout filename
  - create `<output-root>/<scene-stem>/`
  - write `scene_authoring.json`
  - compile `scene_runtime.json` immediately through shared `core_scene_compile`
  - preserve the resulting authoring/runtime paths for UI diagnostics/logging
- The scene-project export integration now has an explicit tool/API entrypoint
  that writes canonical `scene_authoring.json`, compiled `scene_runtime.json`,
  `scene_project.json`, `object_manifest.json`, and empty downstream scaffold
  directories into one selected project root while preserving ordinary scene
  export behavior. Project metadata uses root-relative paths, and
  `scene_authoring.json` remains the editable LineDrawing import target;
  `scene_runtime.json` remains compiled output only.
- Scene-project export now populates `object_manifest.json` for live
  `mesh_asset_instance` objects that reference runtime mesh sidecars. The
  manifest records stable object ids, display names, object kind, mesh/source
  asset ids, vertex/triangle counts, extension presence flags, and
  project-relative `assets/mesh_assets/*.runtime.json` sidecar paths. Referenced
  runtime mesh sidecars are copied into the project `assets/mesh_assets/`
  folder during export, while `scene_authoring.json` remains the editable import
  target and `scene_runtime.json` remains compiled output only.
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
  - for mesh-object probes, include `mesh_asset_instance` entries with copied
    runtime mesh sidecars rather than hand-editing `scene_runtime.json`
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
  - `Core`, `Editor`, `Input`, `Layout`, `Math`, `Menu`,
    `ObjectAuthoring`, `Render`, `Tools`, `UI`, `app`

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
  - export destinations come from the configured output root; active scene
    paths only influence the generated scene name
  - stored full-3D plane primitive metadata is normalized to the plane-locked
    scene contract during canonical export so older platform-plane layouts
    still compile

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
- Source visual proof:
  - `make -C line_drawing visual-artifact`
  - renders one menu-first source-runtime frame through SDL/Vulkan and writes
    `line_drawing/visual_artifacts/line_drawing_first_frame.bmp`
  - expected success line:
    `visual-artifact: <absolute artifact path>`
  - `make -C line_drawing visual-artifact-editor`
  - enters the existing editor surface, renders one viewport/editor frame
    through SDL/Vulkan, and writes
    `line_drawing/visual_artifacts/line_drawing_editor_first_frame.bmp`
  - expected success line:
    `visual-artifact-editor: <absolute artifact path>`
  - requires local display-session access; if SDL reports no displays, rerun
    from a GUI-capable session rather than treating it as app logic failure
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
