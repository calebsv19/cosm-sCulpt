# LineDrawing

LineDrawing is an SDL2-based sketcher for layout/geometry prototyping. It now supports snap-to-grid wall drafting, bezier-curved anchors (with editable handles), multi-anchor selection/dragging, JSON persistence, and an overlay UI that can host diagnostics or future sensor readouts.

## Run-time Flow
1. `src/main.c` boots the SDL app framework, loads fonts, and initialises `GlobalState` (grid, layout, editor, UI).
2. The custom SDL loop (`App_Run`) pumps events into the input system, calls `Global_TickSystems` to clean layout data, and triggers `Render_Frame`.
3. Rendering draws the grid, layout geometry, editor overlays, and UI panel on each frame.

## Module Map
- `config/` — persisted layout JSON; provides the default anchors and walls and acts as the save file for edits.
- `src/Core/` — wraps SDL initialisation and owns `GlobalState`, which other modules query via `Global_Get()`.
- `src/Input/` — mouse/keyboard handlers that drive the grid camera, lasso or click selections, multi-anchor dragging, and wall placement.
- `src/Layout/` — anchor/wall data model (including bezier handle metadata), JSON IO, hitbox rebuilds, and drawing logic.
- `src/Layout/Grid/` — camera-style pan/zoom state and grid rendering helpers.
- `src/Render/` — frame compositor that orders grid, layout, editor, and UI drawing.
- `src/UI/` — UI panel/buttons, top-of-screen info overlay, lasso/selection visuals, click handling, and the font manager.
- `src/Editor/` — wall placement workflow, multi-selection state (including undo/redo snapshots), bezier handle tracking, and editor overlays (ghost walls + selection marquee).
- `src/Math/` — lightweight vector helpers used by layout, grid, and editor code.
- `external/` — third-party libraries (currently cJSON) compiled in by the makefile.
- `src/Tools/` — reusable tooling code; houses `ShapeLib/` (pure shape structs + bezier flattening + JSON IO), the Layout→Shape bridge, and helpers shared with other programs.
- `export/` — auto-created when exporting; stores Shape JSON assets that downstream tools can consume. Run `make export-assets` to convert everything under `export/` into canonical ShapeAssets inside the shared directory (defaults to `shared/assets/shapes`, override with `SHAPE_ASSET_DIR`).
- `include/` — project assets such as fonts that the font manager loads.
- `tests/` — lightweight C test harness and suites covering math and layout behaviour.

## Build & Run
This project targets SDL2 + SDL2_ttf and is built with `make`.

```sh
make            # builds the LineDrawing binary into build/bin/LineDrawing
make run        # builds then runs the application
make debug=1    # builds with debug flags
make clean      # removes build artifacts
```

Compiler and linker flags are pulled from `sdl2-config`, and `external/cjson/cJSON.c` is compiled alongside the in-tree sources.

### Testing

Run the automated checks with:

```sh
make test       # builds lib objects and executes build/tests/bin/run_tests
```

The test harness links against the same objects as the runtime (minus `src/main.c`) so behavioural drift is caught quickly.

### Shape Export Tooling

The same shape conversion pipeline that powers the in-app Export button is available as a CLI helper:

```sh
# Convert a layout JSON into export/<name>.json
build/bin/shape_tool config/airfoil_basic.json --export-shape airfoil.json

# Convert using a specific projection plane (xy|yz|xz)
build/bin/shape_tool config/airfoil_basic.json --plane yz --export-shape airfoil_yz.json

# Preview the resulting geometry in an SDL window
build/bin/shape_tool config/airfoil_basic.json --view
```

All exported assets are written to `export/` regardless of the path you pass after `--export-shape`, which keeps them easy to find even when sharing between projects.

## Editor Shortcuts & UI
- `Ctrl+Z` / `Cmd+Z` — undo the last layout mutation (wall/anchor edits, pin toggles, origin shifts, JSON loads).
- `Ctrl+Shift+Z` or `Ctrl+Y` — redo.
- `1` / `2` / `3` — switch active edit plane (`XY`, `YZ`, `XZ`).
- `[` / `]` — move active plane offset by one grid unit (hold `Shift` for 10x step).
- `V` — toggle between `PLANE_VIEW` and `FREE_VIEW`.
- In `FREE_VIEW`: `Q`/`E` yaw, `T`/`G` pitch, `I`/`K` move view target up/down, `J`/`L` move view target left/right.
- In `FREE_VIEW`: hold `Alt/Option` and move the mouse to orbit around the layout centroid (no click required).
- `O` — recenter the grid to the selected anchor.
- `P` — toggle the selected anchor's persistence.
- `C` — toggle the selected anchor between sharp corner and smooth curve (requires exactly two connected walls).
- `L` — toggle whether the selected curved anchor's bezier handles are linked (symmetric) or independent.
- `Delete` / `Backspace` — remove the selected wall or anchor.
- `Shift` + click — add/remove anchors to a multi-selection. Dragging with shift held over empty space draws a translucent marquee to select anchors inside the box.
- `Alt` + drag — disables grid snapping for the anchor currently being dragged (other selected anchors follow the same delta without snapping).
- Double-click a selected anchor — collapse the multi-selection down to that anchor (single drag target).
- `Save JSON` button — opens a naming dialog that writes to `config/<name>.json` (layout changes prompt for a new file name).
- `Load JSON` button — exposes a dropdown of every `.json` layout in `config/` for quick swapping between floor plans.
- `Export Shape` button — converts the in-memory Layout into a canonical Shape asset and writes it to `export/<current config name>.json` using the shared ShapeLib pipeline (no dialog required). Export flattening uses the current active plane (`XY`/`YZ`/`XZ`).

Selection details (position, connections, bezier handle lengths/angles, drag mode, group count, delete mode) appear in the top overlay, while action buttons sit below it to keep the workspace tidy. Selected anchors glow while dragging, bezier handles render with hover/selection feedback, and the marquee indicates the lasso bounds.
In `PLANE_VIEW`, the background grid is rendered for plane editing. In `FREE_VIEW`, the background grid is hidden and a world-axis gizmo (+X red, +Y green, +Z blue) is rendered around the layout centroid for orientation.

## Persisted Data
Layout edits are stored in `config/layout_config.json`, which encodes:
- `file`: metadata about the save (`schemaVersion` and `gridSize`). Files saved with a future schema version are rejected to avoid corrupting the current runtime.
- `anchors`: world-space coordinates (`x`, `y`, `z` floats in schema v4), a `persistent` flag that keeps an anchor alive when auto-prune is enabled, the anchor `type` (`corner` or `curve`), handle linkage flag, handle basis plane (`handleAxis`: `xy|yz|xz`), and polar handle definitions (`handleInLength`, `handleInAngleDeg`, `handleOutLength`, `handleOutAngleDeg`). Older JSONs that only store `x/y` still load with `z = 0`.
- `walls`: index pairs `a`/`b` that connect anchors into wall segments.

The UI panel exposes "Save JSON" and "Load JSON" buttons, so this file is the primary project state shared between sessions.

When you need the simplified Shape format (paths + cubic segments) for other programs, click "Export Shape" (or run `shape_tool --export-shape ...`). The exporter streams the connected wall graph into a ShapeDocument and saves it next to the layout exports in `export/`. For the shared pipeline, run `make export-assets` (respects `SHAPE_ASSET_DIR`) to convert those exports into canonical ShapeAssets for physics/ray-tracing.
