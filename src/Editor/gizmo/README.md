# Editor Gizmo

This subtree owns editor-side gizmo math and target helpers.

## Ownership

- `object_handle_gizmo.c` normalizes selected primitive handles, scene bounds,
  and object-authoring topology selections into legal gizmo targets for render,
  hitbox, and input paths.
- `space_gizmo_drag.c` owns free-view axis drag math and signed distance
  application helpers.

## Boundary

- Gizmo helpers describe legal targets and drag math.
- Layout mutation stays in `src/Layout/`.
- Object-authoring semantic topology changes stay in `src/ObjectAuthoring/`.
- Rendering stays in `src/Editor/render/`.
