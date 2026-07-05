# Layout Asset

This subtree owns layout-side adapters for reusable object assets and imported
mesh asset metadata.

## Ownership

- `layout_object_asset_mesh_authoring.*` maps the editable object workspace
  layout onto the shared `mesh_asset_authoring_v1` primitive-seed contract and
  preserves private `ObjectAuthoring` state in the app-local extension.
- `layout_imported_mesh_asset.*` owns layout-facing imported mesh asset
  metadata and handoff helpers.

## Boundary

- Shared mesh semantics live in vendored/shared mesh libraries.
- Object/CAD operation history lives in `src/ObjectAuthoring/`.
- Scene placement of runtime mesh sidecars lives in `src/Layout/scene/`.
