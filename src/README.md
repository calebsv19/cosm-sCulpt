# Source Overview

All engine code lives under `src/`, separated by responsibility so the floor-plan editor can grow into a richer home-monitoring tool.

## Entry Point
- `main.c` — initialises SDL via the app framework, loads fonts, allocates `GlobalState`, registers callbacks, sets a throttled render mode (60 FPS), and orchestrates shutdown.

## Update Loop
1. SDL events are routed through `Input_Handle`, which dispatches to mouse and keyboard handlers, updates the grid camera, and mutates layout/editor state.
2. `Global_TickSystems` owns update/mutation processing and seeds a typed update frame contract.
3. `Render_DeriveFrame` builds a frame-visible render contract (`LineDrawingRenderDeriveFrame`) from update state.
4. `Render_SubmitFrame` consumes the derive contract and performs draw/submit calls (grid, layout, editor overlays, UI).

## Module Directories
- `Core/` — SDL bootstrapping and the project-wide `GlobalState` singleton.
- `Input/` — user input handling split across devices (mouse/keyboard) plus marquee selection and multi-anchor drag plumbing.
- `Layout/` — geometry data structures, bezier-aware rendering, hit detection (including handle hitboxes), and JSON persistence.
- `Layout/Grid/` — grid camera maths and rendering helpers shared by layout and editor code.
- `Render/` — top-level frame compositor.
- `UI/` — UI panel data, rendering, click routing, and font management.
- `Editor/` — wall-placement state machine, bezier handle/link tracking, multi-selection data structures, and overlay rendering (ghost walls + selection marquee).
- `Math/` — inline vector utilities and coordinate transforms.
- `Tools/` — standalone helpers that are also linked into the runtime (ShapeLib core structs/flatteners/JSON, the Layout→Shape adapter, canonical scene exporter, SDL previewer, diagnostics dataset mapping, and CLI tools `shape_tool` + `shape_pack_tool`). This folder keeps the export pipeline reusable across applications.

Each directory below has its own README with per-file details and relationships.
