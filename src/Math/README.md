# Math Helpers

Inline math utilities shared by layout, grid, and editor code.

## Files
- `math_util.h` — declares a simple `Vec2` type plus helper functions for distance, snapping to the grid, converting between world and screen coordinates using the grid state, and bezier-friendly helpers (polar vector creation, degree/radian conversion, handle offset helpers, angle normalisation). These utilities are used by layout rendering, bezier handle seeding, and the input layer when computing drag deltas.

## Usage
These functions keep coordinate handling consistent across the project: layout rendering, editor overlays, hitbox calculations, and input conversions all rely on them.
