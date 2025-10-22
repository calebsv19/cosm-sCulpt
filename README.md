# LineDrawing

LineDrawing is an SDL2-based floor-plan sketcher used to lay out rooms and anchor points for a planned home heat-map display. The app lets you draw snap-to-grid walls, persist the geometry, and render a UI overlay that will later host sensor readings.

## Run-time Flow
1. `src/main.c` boots the SDL app framework, loads fonts, and initialises `GlobalState` (grid, layout, editor, UI).
2. The custom SDL loop (`App_Run`) pumps events into the input system, calls `Global_TickSystems` to clean layout data, and triggers `Render_Frame`.
3. Rendering draws the grid, layout geometry, editor overlays, and UI panel on each frame.

## Module Map
- `config/` — persisted layout JSON; provides the default anchors and walls and acts as the save file for edits.
- `src/Core/` — wraps SDL initialisation and owns `GlobalState`, which other modules query via `Global_Get()`.
- `src/Input/` — mouse/keyboard handlers that manipulate the grid, layout, and editor selections.
- `src/Layout/` — anchor/wall data model, JSON IO, hitbox rebuilds, and drawing logic.
- `src/Layout/Grid/` — camera-style pan/zoom state and grid rendering helpers.
- `src/Render/` — frame compositor that orders grid, layout, editor, and UI drawing.
- `src/UI/` — UI panel definition, rendering, click handling, and the font manager.
- `src/Editor/` — two-click wall placement workflow, selection state, and editor overlays.
- `src/Math/` — lightweight vector helpers used by layout, grid, and editor code.
- `external/` — third-party libraries (currently cJSON) compiled in by the makefile.
- `include/` — project assets such as fonts that the font manager loads.

## Build & Run
This project targets SDL2 + SDL2_ttf and is built with `make`.

```sh
make        # builds the LineDrawing binary into ./LineDrawing
make run    # builds then runs the application
make clean  # removes build artifacts
```

Compiler and linker flags are pulled from `sdl2-config`, and `external/cjson/cJSON.c` is compiled alongside the in-tree sources.

## Persisted Data
Layout edits are stored in `config/layout_config.json`, which encodes:
- `anchors`: world-space coordinates (floats) plus a `persistent` flag that keeps an anchor alive when auto-prune is enabled.
- `walls`: index pairs `a`/`b` that connect anchors into wall segments.

The UI panel exposes "Save JSON" and "Load JSON" buttons, so this file is the primary project state shared between sessions.
