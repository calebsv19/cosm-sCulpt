# Render Module

This module coordinates the order of draw calls each frame.

## Files
- `render_handler.h` / `render_handler.c` — expose explicit RS1 phase seams:
  - `Render_DeriveFrame(...)` builds `LineDrawingRenderDeriveFrame` (view context, state pointers, palette/background, screen metrics, draw baseline counters).
  - `Render_SubmitFrame(...)` consumes derive output and performs draw/submit calls in stable order.
  - `Render_Frame(...)` remains as a compatibility wrapper over derive+submit.
  The submit phase clears the renderer, draws the plane grid in `PLANE_VIEW`, renders layout geometry (including bezier segments + handle gizmos), draws a free-view world-axis gizmo (+X/+Y/+Z) around the layout centroid when `FREE_VIEW` is enabled, overlays editor visuals (anchor highlight, ghost wall, marquee rectangle), paints the info overlay bar, and finally draws the UI panel.

## Interactions
- Consumes top-level update contracts from `main.c` via `LineDrawingUpdateFrame`.
- Uses renderer helpers exposed by `Layout`, `Editor`, and `UI` submodules so those systems stay responsible for their own visuals.
