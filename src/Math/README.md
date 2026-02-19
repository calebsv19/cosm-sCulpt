# Math Helpers

Inline math utilities shared by layout, grid, and editor code.

## Files
- `math_util.h` — declares `Vec2` and the new 3D foundation types (`Vec3`, `Ray3`, `Plane3`, `PlaneFrame3`) with helper functions for:
  - 2D operations (distance, snapping, world/screen transforms, bezier polar helpers).
  - 3D vector operations (dot/cross/length/normalize/add/sub/scale).
  - Plane math (`Plane3_From*`, signed distance, point projection).
  - Ray-plane intersection (`Ray3_IntersectPlane`).
  - Plane-local projection helpers (`PlaneFrame3_*`) used to flatten 3D points into a 2D editing basis.
  - Compatibility adapters (`Vec2 <-> Vec3`) used during migration.
  - Active-plane helpers for plane-constrained workflows (`ViewPlane`, `Vec3_ProjectToPlane`, `Vec3_FromPlaneCoords`, `ScreenToPlaneWorld`).
  - Plane-local handle conversion helpers (`Vec3_HandleOffsetFromPlanePolar`, `Vec3_HandleWorldPosition`, `Vec3_HandlePolarFromWorldDelta`) shared by render/hit-test/input.

## Usage
These functions keep coordinate handling consistent across the project. Existing 2D systems continue to use the `Vec2` path, while Phase 1 introduces a 3D-capable math layer that future phases will adopt for world storage, plane-constrained editing, and 3D-aware picking/rendering.
