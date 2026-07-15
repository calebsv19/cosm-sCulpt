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
- `layout_mesh_solid_preview.*` owns the LineDrawing-local filled and
  outline-only runtime-mesh preview. It obtains coherent vertex-cluster
  interactive/settled LODs from shared `core_mesh_preview` `0.5.0` through a
  thin adapter over the canonical `core_mesh_asset` document, then rasterizes
  them through a CPU depth
  buffer, adds view-dependent silhouette/depth outlines, and uploads one cached
  viewport texture. Orbit direction changes or scene-geometry changes use an
  8,000-triangle/60%-scale interaction tier; a stable view promotes to the
  18,000-triangle/75%-scale preview without mutating canonical mesh vertices,
  triangles, ids, or export state. Zoom, pan, resize, and plane-offset changes
  reraster the established tier without lowering it. Material appearance also
  rerasterizes at the current tier, while selection and hover remain later
  viewport overlays and do not recolor, reraster, or demote the cached surface.
  Wire composes its transparent view-dependent silhouette/depth outline with
  the shared `core_mesh_preview` feature edges, so smooth complex meshes remain
  recognizable without turning Wire into a filled-surface mode.
- `layout_object_faces.*` owns primitive face labels and object-mode face-pick
  helpers.

## Boundary

- This subtree owns scene/runtime layout state, not reusable object-authoring
  documents.
- The solid/outline preview is an app-local editor visualization, not downstream
  simulation or final RayTracing material authority. Shared code owns only the
  renderer-neutral LOD mesh; native GPU depth rendering, smooth
  normals/material bindings, display policy, and interaction overlays remain
  consumer-owned.
