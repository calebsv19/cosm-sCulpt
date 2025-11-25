# Render Module

This module coordinates the order of draw calls each frame.

## Files
- `render_handler.h` / `render_handler.c` — expose `Render_Frame`, the single entry point called from the main loop. The implementation fetches `GlobalState`, ensures hitboxes are rebuilt if marked dirty, clears the renderer, draws the grid (unit and five-unit lines), renders the layout geometry (including bezier segments + handle gizmos), overlays the editor (anchor highlight, ghost wall, marquee rectangle), paints the info overlay bar, and finally draws the UI panel before presenting the frame.

## Interactions
- Relies on `Global_Get()` to access the grid, layout, and editor state.
- Uses renderer helpers exposed by `Layout`, `Editor`, and `UI` submodules so those systems stay responsible for their own visuals.
