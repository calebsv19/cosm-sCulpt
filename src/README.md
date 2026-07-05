# Source Overview

All application code lives under `src/`, separated by responsibility so
`sCulpt` can keep host-menu, editor, scene authoring, object/CAD authoring,
import/export tooling, and runtime proof lanes easy to route.

## Entry Point
- `main.c` — delegates into the scaffold wrapper by calling
  `line_drawing_app_main(...)`.
- `app/` — owns the lifecycle wrapper shell behind
  `include/line_drawing/line_drawing_app_main.h`, including bootstrap,
  config-load, state-seed, subsystem-init, runtime-start, run-loop handoff, and
  shutdown sequencing. See `app/README.md`.

## Update Loop
1. SDL events are routed through `Input_Handle`, which dispatches to mouse and keyboard handlers, updates the grid camera, and mutates layout/editor state.
2. `Global_TickSystems` owns update/mutation processing and seeds a typed update frame contract.
3. `Render_DeriveFrame` builds a frame-visible render contract (`LineDrawingRenderDeriveFrame`) from update state.
4. `Render_SubmitFrame` consumes the derive contract and performs draw/submit calls (grid, layout, editor overlays, UI).

## Module Directories
- `Core/` — app-wide `GlobalState`, runtime paths, recent contexts, file
  catalog, pane host, workspace handoff, SDL loop helpers, and mode adapters.
- `Input/` — SDL event routing, keyboard/mouse handling, input-policy seams,
  viewport picking, hover policy, and drag-session plumbing. Dense mouse-drag
  ownership is summarized in `Input/mouse/README.md`.
- `Layout/` — layout/scene data structures, primitives, imported mesh asset
  state, mesh asset instances, hit detection, JSON persistence, and layout
  rendering. Durable sub-lanes under `Layout/asset`, `Layout/primitives`, and
  `Layout/scene` now have local README maps.
- `Layout/Grid/` — grid camera maths and rendering helpers shared by layout and editor code.
- `Menu/` — app-local top-level host menu state, input handling, and rendering for the menu-first launch surface.
  - current scope includes the first Phase 2 scene catalog shell for layouts/scenes under the input root
- `ObjectAuthoring/` — reusable object/CAD authoring documents, sessions,
  stable topology ids, operation replay, persistence, and app-local runtime mesh
  compile.
- `Render/` — top-level frame compositor.
- `UI/` — editor shell panels, File pane/browser, scene/object/view/create
  panes, topbar, overlays, workspace-authoring host, text/font/theme support,
  and click routing. Dense lanes under `UI/panel`, `UI/overlay`, and
  `UI/topbar` now have local README maps.
- `Editor/` — editor state, wall/anchor workflows, object/scene-bounds gizmo
  selections, object face/sketch/extrude controls, primitive preview, and
  editor overlay rendering. `Editor/gizmo` and `Editor/render` document the
  durable helper sub-lanes.
- `Math/` — inline vector utilities and coordinate transforms.
- `Tools/` — reusable export/tooling code, including ShapeLib, shape
  diagnostics, scene export/import, canonical scene export, imported mesh
  harness, agent scene tooling, and sema/toolchain dump customers. See
  `Tools/README.md`.

Major source directories and durable dense sub-lanes have local README files
with per-file details and ownership boundaries.
