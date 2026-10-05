# Sculpt (`line_drawing`)

`Sculpt` is the packaged desktop product for the `line_drawing` program. It is
an SDL2-based geometry, scene-authoring, and object/CAD prototyping workspace.
It supports snap-to-grid wall drafting, bezier-curved anchors, multi-anchor
selection/dragging, JSON persistence, a menu-first host shell, a tabbed editor
shell, scene export, reusable object authoring, and imported/runtime mesh asset
workflows.

The [illustrative van workflow](docs/illustrative_van_workflow.md) combines the
physical layout, semantic assemblies, relationships and movement checks into a
saved teaching project. All vehicle dimensions are placeholders. Side/front/free views display +Z upward.
Selected gizmos keep a bounded viewport-relative screen size across zoom levels.
Use **View → Select** to pick visible wireframe edges or object centers; **Orbit**
and **Pan** expose navigation without keyboard shortcuts. Measure reference names
use full-width controls, and motion bounds labels appear in a viewport legend.

## Docs
- docs index: `docs/README.md`
- current state: `docs/current_truth.md`
- future intent: `docs/future_intent.md`
- keybinds: `docs/KEYBINDINGS.md`

Identity note:

- public product name: `Sculpt`
- repository/program key: `line_drawing`
- launcher/log/runtime env paths still use `LineDrawing` or `line_drawing`
  identifiers where those are part of the current technical contract

## Run-time Flow
1. `src/main.c` delegates into `line_drawing_app_main(...)` through the
   scaffold wrapper in `src/app/`.
2. The wrapper owns lifecycle stage sequencing, then hands off to the existing
   SDL runtime session.
3. Runtime starts menu-first: the host `MENU` surface owns high-level
   launch/reopen/catalog flow, while the `EDITOR` surface owns the interactive
   authoring workspace.
4. The custom SDL loop (`App_Run`) pumps events into input routing, calls
   `Global_TickSystems` for update/mutation processing, derives a
   frame-visible render contract, and submits the frame through the render
   pipeline.

## Module Map
- `config/` — persisted layout JSON; provides the default anchors and walls and acts as the save file for edits.
- `src/app/` — lifecycle wrapper shell behind `line_drawing_app_main(...)`.
- `src/Core/` — owns `GlobalState`, data paths, recent contexts, file catalog,
  pane host, workspace handoff, SDL loop helpers, and mode adapters.
- `src/Input/` — mouse/keyboard handlers, input-policy seams, viewport picking,
  hover policy, and drag-session plumbing.
- `src/Layout/` — anchor/wall data model, 3D scene/object state, primitive and
  imported mesh asset lanes, JSON IO, hitbox rebuilds, and drawing logic.
- `src/Layout/Grid/` — camera-style pan/zoom state and grid rendering helpers.
- `src/Menu/` — menu-first host shell, recents, root browsing, layout/scene
  catalog previews, and high-level editor handoff.
- `src/ObjectAuthoring/` — reusable object/CAD authoring documents, stable ids,
  operation replay/evaluation, persistence, and runtime mesh compile.
- `src/Render/` — frame compositor that orders grid, layout, editor, and UI drawing.
- `src/UI/` — topbar, editor panel shell, File pane/browser, scene/object/view/create panes, overlays, click routing, and font/theme support.
- `src/Editor/` — wall placement workflow, multi-selection state (including undo/redo snapshots), bezier handle tracking, and editor overlays (ghost walls + selection marquee).
- `src/Math/` — lightweight vector helpers used by layout, grid, and editor code.
- `external/` — third-party libraries (currently cJSON) compiled in by the makefile.
- `src/Tools/` — reusable tooling code; houses `ShapeLib/`, Layout→Shape
  export, diagnostics pack/trace tooling, canonical scene export/import,
  atomic scene-directory export with content-addressed runtime-mesh payloads
  through shared `core_scene_compile`, imported mesh
  harnesses, and agent-scene tooling.
- `export/` — auto-created when exporting; stores Shape JSON assets that downstream tools can consume. Run `make export-assets` to convert everything under `export/` into canonical ShapeAssets inside the shared directory (defaults to `shared/assets/shapes`, override with `SHAPE_ASSET_DIR`).
- `include/` — project assets such as fonts that the font manager loads.
- `tests/` — lightweight C/Python/shell test harnesses covering math, layout,
  object authoring, scene export, host menu/catalogs, File pane/browser,
  imported mesh harnesses, UI panels, and agent-scene tooling.

## Build & Run
This project targets SDL2 + SDL2_ttf and is built with `make`.

```sh
make            # builds the LineDrawing binary into build/toolchains/clang/bin/LineDrawing
make run        # builds then runs the application
make debug=1    # builds with debug flags
make clean      # removes build artifacts
```

Canonical packaging and Desktop refresh flows produce `sCulpt.app`. The
persistent development lane separately produces `sCulpt Main Edit.app` with
an isolated bundle, runtime, and log identity. See `docs/desktop_packaging.md`
and `docs/main_edit_worktree.md`.

Compiler and linker flags are pulled from `sdl2-config`, and `external/cjson/cJSON.c` is compiled alongside the in-tree sources.

### Testing

Run the automated checks with:

```sh
make test       # builds host test objects and executes build/host/tests/bin/run_tests
```

The test harness links against the same objects as the runtime (minus `src/main.c`) so behavioural drift is caught quickly.

For source-run first-frame visual proofs, run:

```sh
make visual-artifact          # menu-first source frame
make visual-artifact-editor   # editor viewport/source frame
```

These targets launch the development runtime in a one-shot capture mode and
write ignored BMPs under `visual_artifacts/`. The editor proof enters the
existing editor surface before capture, so it includes the viewport, topbar,
and panels without changing scene, CAD, STL, or export behavior. Both commands
require access to the local display session.

### Shape Export Tooling

The same shape conversion pipeline that powers the in-app Export button is available as a CLI helper:

```sh
# Convert a layout JSON into export/<name>.json
build/toolchains/clang/bin/shape_tool config/airfoil_basic.json --export-shape airfoil.json

# Convert using a specific projection plane (xy|yz|xz)
build/toolchains/clang/bin/shape_tool config/airfoil_basic.json --plane yz --export-shape airfoil_yz.json

# Preview the resulting geometry in an SDL window
build/toolchains/clang/bin/shape_tool config/airfoil_basic.json --view
```

All exported assets are written to `export/` regardless of the path you pass after `--export-shape`, which keeps them easy to find even when sharing between projects.

### Shape Diagnostics Pack Tooling

`line_drawing3d` now includes an additive diagnostics export path based on shared `core_data` + `core_pack`.

```sh
# Build the diagnostics pack tool
make shape_pack_tool

# Export a layout snapshot as a diagnostics .pack file
make shape_to_pack LAYOUT=config/layout_config.json PACK=export/layout_diag.pack AXIS=xy
```

Pack output includes:
- `LDHD` header chunk (schema + counts)
- `LDMJ` metadata JSON (`profile`, `schema_family`, `schema_variant`, axis, bounds, counts)
- `LDAN` shared base anchor rows (`x`, `y`, handle values, persistent/type)
- `LDWL` compact wall rows
- `LDA3` additive 3D extension rows (`z`, `handle_axis`)

This does not replace existing JSON export paths; it is additive for shared-pipeline diagnostics and cross-program inspection.

### Runtime Import Policy (Updated 2026-05-06)

Runtime layout loading now supports two authoring sources:

- `Load JSON`: `.json` layout files from a chosen JSON root, shown through the in-app scrollable picker
- `Load Package`: scene directories under a chosen scene root that contain editable `scene_authoring.json`; `scene_package.json` is the preferred entrypoint, while legacy authoring-only directories remain readable. A runtime export is not required before discovery.
- Rejected runtime sources: `.pack` diagnostics artifacts and compiled `scene_runtime.json` files
- `.pack` remains tooling-only for diagnostics and cross-program inspection

### Shape Trace Tooling (Slice 2, 2026-03-10)

Trace tooling is now aligned for 2D/3D with the same CLI and output contract.

```sh
make shape_trace_tool
make shape_to_trace SHAPE=export/example.json TRACE=export/example_trace_v0.pack
make shape_to_trace_batch
```

Tool output lanes:
- `seg_type` (0=line, 1=cubic)
- `seg_len` (approx segment length)
- `path_index`
- `segment_index`

Tool marker lane:
- `shape_marker` (profile/shape/path boundaries)

### Shape Dataset Schema Parity (Slice 3, 2026-03-10)

`core_data` schema parity is now locked between 2D and 3D for shared tables/metadata.

Shared metadata keys:
- `profile` (`line_drawing_shape_diag_v1`)
- `schema_family` (`line_drawing_shape_diag`)
- `schema_variant` (`3d` for this app, `2d` in the 2D app)
- `schema_version`, `projection_axis`
- `active_anchor_count`, `active_wall_count`, `curved_anchor_count`, `persistent_anchor_count`
- `bounds_min_x`, `bounds_min_y`, `bounds_min_z`, `bounds_max_x`, `bounds_max_y`, `bounds_max_z`

Shared typed tables:
- `anchors_v1` (`x`, `y`, `persistent`, `anchor_type`, `handle_in_length`, `handle_in_angle_deg`, `handle_out_length`, `handle_out_angle_deg`)
- `walls_v1` (`a`, `b`, `lock_length`)

3D-only additive extension table:
- `anchors_3d_ext_v1` (`anchor_index`, `z`, `handle_axis`)

### Shape Pack Contract Parity (Slice 4, 2026-03-10)

`core_pack` diagnostics contract is now aligned for 2D/3D:
- same chunk sequence: `LDHD`, `LDMJ`, `LDAN`, `LDWL`, `LDA3`
- same `LDAN` base row binary layout across both apps
- `LDA3` carries additive 3D extension payload (`z`, `handle_axis`)

### Core IO Cleanup (Slice 5, 2026-03-10)

Low-risk theme preset persistence paths now use shared `core_io`:
- path-exists checks use `core_io_path_exists`
- preset load/save uses `core_io_read_all` / `core_io_write_all`
- runtime behavior remains unchanged (single preset-name line with trailing newline trimmed on load)


## Editor Shortcuts & UI
- `Ctrl+Z` / `Cmd+Z` — undo the last layout mutation (wall/anchor edits, pin toggles, origin shifts, JSON loads).
- `Ctrl+Shift+Z` or `Ctrl+Y` — redo.
- `1` / `2` / `3` — switch active edit plane (`XY`, `YZ`, `XZ`).
- `[` / `]` — move active plane offset by one grid unit (hold `Shift` for 10x step).
- `V` — toggle between `PLANE_VIEW` and `FREE_VIEW`.
- In `FREE_VIEW`: `Q`/`E` yaw, `T`/`G` pitch, `I`/`K` move view target up/down, `J`/`L` move view target left/right.
- In `FREE_VIEW`: `Alt/Option` + left drag orbits around the durable view target; middle-button drag pans in the camera screen basis; the mouse wheel zooms around the pointer; and `F` frames the selected 3D object or the full scene.
- `O` — recenter the grid to the selected anchor.
- `P` — toggle the selected anchor's persistence.
- `C` — toggle the selected anchor between sharp corner and smooth curve (requires exactly two connected walls).
- `L` — toggle whether the selected curved anchor's bezier handles are linked (symmetric) or independent.
- `Delete` / `Backspace` — remove the selected wall or anchor.
- `Shift` + click — add/remove anchors to a multi-selection. Dragging with shift held over empty space draws a translucent marquee to select anchors inside the box.
- `Alt` + drag — disables grid snapping for the anchor currently being dragged (other selected anchors follow the same delta without snapping).
- Double-click a selected anchor — collapse the multi-selection down to that anchor (single drag target).
- `Save` button — updates the active layout JSON, canonical scene-authoring document, or object asset in place. Saving a scene changes authored truth only and does not rebuild `scene_runtime.json`.
- `Scene Save As` button — writes a new named layout/object document, or creates a new sibling scene package from loaded authoring. Scene Save As refuses to overwrite an existing package, preserves only authoring-owned `assets/` and `attachments/`, leaves the original unchanged, writes a new `scene_package.json`, and omits runtime, dependencies, and receipts until explicit export.
- `Load JSON` button — opens a native folder picker for the JSON root, then shows a clipped, scrollable in-app list of every `.json` file in that directory for quick swapping between layouts.
- `Load Package` button — opens a native folder picker for the scene root, then shows a clipped, scrollable in-app list of authoring packages sourced from that folder. Scene discovery accepts a root that is itself a package or one grouped layer deeper (`group/scene`). Each loaded package resolves through `scene_package.json` when present and restores editable state from `scene_authoring.json`.
- `Export Shape` button — converts the in-memory Layout into a canonical Shape asset and writes it to `export/<current config name>.json` using the shared ShapeLib pipeline (no dialog required). Export flattening uses the current active plane (`XY`/`YZ`/`XZ`).
- `Export Runtime` button — atomically publishes one new named package directory under the configured output root with `scene_package.json`, `scene_authoring.json`, deterministic `scene_runtime.json`, canonical `scene_dependencies.json`, content-addressed dependencies, and `scene_export_receipt.json`. The receipt binds the package entrypoint and all compiled artifacts. Publication is create-only, so an existing iteration is never replaced. Export remains a derived-output action: it does not replace the active authoring identity or clear unsaved authoring state.

Selection details (position, connections, bezier handle lengths/angles, drag mode, group count, delete mode) appear in the top overlay, while action buttons sit below it to keep the workspace tidy. Selected anchors glow while dragging, bezier handles render with hover/selection feedback, and the marquee indicates the lasso bounds.
In `PLANE_VIEW`, the background grid is rendered for plane editing. In `FREE_VIEW`, the background grid is hidden and a world-axis gizmo (+X red, +Y green, +Z blue) is rendered around the layout centroid for orientation.

## Persisted Data
Layout edits are stored in `config/layout_config.json`, which encodes:
- `file`: metadata about the save (`schemaVersion` and `gridSize`). Files saved with a future schema version are rejected to avoid corrupting the current runtime.
- `anchors`: world-space coordinates (`x`, `y`, `z` floats in schema v4), a `persistent` flag that keeps an anchor alive when auto-prune is enabled, the anchor `type` (`corner` or `curve`), handle linkage flag, handle basis plane (`handleAxis`: `xy|yz|xz`), and polar handle definitions (`handleInLength`, `handleInAngleDeg`, `handleOutLength`, `handleOutAngleDeg`). Older JSONs that only store `x/y` still load with `z = 0`.
- `walls`: index pairs `a`/`b` that connect anchors into wall segments.

The UI panel exposes explicit authoring save, Scene Save As, Load Layout, Load
Package, and Export Runtime actions. Layout JSON remains the lightweight
line-drawing document, while `scene_package.json` identifies a scene package,
`scene_authoring.json` is editable canonical scene truth and
`scene_runtime.json` is derived compiled output.

When you need the simplified Shape format (paths + cubic segments) for other programs, click "Export Shape" (or run `shape_tool --export-shape ...`). The exporter streams the connected wall graph into a ShapeDocument and saves it next to the layout exports in `export/`. For the shared pipeline, run `make export-assets` (respects `SHAPE_ASSET_DIR`) to convert those exports into canonical ShapeAssets for physics/ray-tracing.

## Typed surface graph producer (M6 source candidate)

Agent scene plane, rect_prism and mesh_asset_instance objects accept a
`surface_graph` object directly or in `ray_tracing`. Required declaration:
version 1, required_capability `optic.surface_graph_v1`, color_space `linear`.
The producer preserves the complete graph, IDs and optional metadata into both
scene_authoring and scene_runtime outputs. Do not combine it with a material
prompt or texture stack. The producer validates its envelope; optiC's
`--validate-surface-graph` and ordinary scene preflight validate node execution,
connections and source/geometry compatibility. Unknown required capabilities
fail; this is not Sculpts preview execution of arbitrary shaders. See optiC's
`docs/surface_material_m6_contract.md` for the node contract and source limits.
The M6 deterministic producer fixture is generated by optiC's
`tests/integration/test_surface_graph_m6.py --sculpts-tool <binary>`.
No desktop package or release is claimed by this source adapter.

The spatial workflow now includes [S5A physical routes](docs/physical_routes.md)
with a Routes tab, native undo/persistence and structured headless editing. The
[corrected narrow ProMaster layout](docs/van_layout_narrow_v2.md) and
[measurement checklist](docs/van_measurement_checklist.md) define the current
provisional van draft.
