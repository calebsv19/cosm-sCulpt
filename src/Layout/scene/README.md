# Layout Scene

This subtree owns layout-level 3D scene state, scene objects, mesh instances,
and scene helper contracts.

## Ownership

- `layout_scene3d.c` owns scene bounds, construction-plane state, and
  bounds-edit helper APIs.
- `layout_object_store.c` owns the stable scene object store for primitive and
  mesh-instance objects.
- `layout_mesh_asset_instance.c` creates transformable runtime mesh sidecar
  instances.
- `layout_mesh_preview_sidecar.*` and `layout_mesh_runtime_preview.*` own
  bounded mesh preview sidecar loading/render data.
- `layout_object_faces.*` owns primitive face labels and object-mode face-pick
  helpers.

## Boundary

- This subtree owns scene/runtime layout state, not reusable object-authoring
  documents.
- Full mesh rendering and downstream simulation/render semantics stay outside
  R0 and outside this subtree until a focused feature slice selects them.
