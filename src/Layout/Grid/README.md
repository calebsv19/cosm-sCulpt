# Grid Module

The grid submodule manages the viewing transform and draws the background grid that the layout snaps to.

## Files
- `grid.h` / `grid.c` — encapsulate grid state: cell size (`gridSize`), world offsets, and zoom scale. Provide `Grid_init` to centre the origin, `Grid_pan` to move the camera in world space, and `Grid_zoom` to zoom while keeping the cursor (or screen centre) anchored.
- `render_grid.h` / `render_grid.c` — render layered grid lines (unit, 5-unit, 10-unit) with configurable draw flags and draw an origin crosshair. Uses floating-point math to avoid drift when panning or zooming.

## Usage
- `GlobalState` owns a single `Grid` instance; input handlers call `Grid_pan`/`Grid_zoom` in response to user actions.
- Rendering invokes `Render_Grid` before drawing the layout so walls/anchors sit on top of the grid background.
- Other modules use helper functions from `Math/math_util.h` to convert between grid world space and screen coordinates using this grid state.
