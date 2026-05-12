# Grid Module

The grid submodule manages the viewing transform and draws the background grid that the layout snaps to.

## Files
- `grid.h` / `grid.c` — encapsulate grid state: cell size (`gridSize`), world offsets, and zoom scale. Provide `Grid_init` to centre the origin, `Grid_pan` to move the camera in world space, `Grid_zoom` for default bounded zoom, and `Grid_zoom_clamped` for app-level policies that need explicit scale limits while keeping the cursor (or screen centre) anchored.
- `render_grid.h` / `render_grid.c` — render layered grid lines (unit, 5-unit, 10-unit) with configurable draw flags and draw an origin crosshair. Uses floating-point math to avoid drift when panning or zooming.

## Usage
- `GlobalState` owns a single `Grid` instance; input handlers call `Grid_pan` directly and route zoom through `Core/viewport_zoom` so enabled scene bounds can lower the zoom-out floor when the projected bounds are larger than the default viewport range. After camera changes, callers raise `Global_FlagGridChanged` so hitboxes rebuild with the new camera transform.
- Rendering invokes `Render_Grid` before drawing the layout so walls/anchors sit on top of the grid background.
- Rendering invokes `Render_Grid` in `PLANE_VIEW` to preserve slice-edit context; in `FREE_VIEW` the background grid is intentionally hidden so orientation comes from the world-axis gizmo.
- Other modules use helper functions from `Math/math_util.h` to convert between grid world space and screen coordinates using this grid state.
