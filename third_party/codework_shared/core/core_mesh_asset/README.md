# core_mesh_asset

Shared reusable 3D mesh-asset contract semantics for the physics trio object-authoring lane.

## Scope
- Typed authored mesh-asset contract helpers for `mesh_asset_authoring_v1`
- Primitive-seed authoring document helpers for the first editable object-asset lane
- Typed runtime mesh-asset contract helpers for `mesh_asset_runtime_v1`
- Shared vocabulary for:
  - asset schema variants
  - asset type
  - authoring source mode
  - primitive seed kind
- Shared validation for:
  - asset identity
  - unit/world-scale pairing
  - pivot/local-frame semantics
  - primitive-seed object payloads
  - runtime mesh counts and local bounds
  - topology expectation flags
- File-backed authoring-document load/save helpers for exporter/importer adapters

## Boundaries
- No mesh editing operations
- No triangulation, extrusion, revolve, or boolean implementation
- No scene-instance ownership (`core_scene` remains the long-term scene envelope owner)
- No render acceleration structures, GPU buffers, solver voxelization, or SDF ownership

## Current Contract (v0.2.0)
- Supported schema variants are exactly:
  - `mesh_asset_authoring_v1`
  - `mesh_asset_runtime_v1`
- Supported asset types are exactly:
  - `solid_mesh`
- Supported primitive seed kinds are exactly:
  - `plane`
  - `rect_prism`
- Supported authoring source modes are exactly:
  - `profile_extrusion`
  - `primitive_seed`
  - `revolve`
- `core_mesh_asset_authoring_contract_validate(...)` currently validates only shared semantic lanes:
  - non-empty `asset_id`
  - known `unit_kind`
  - positive finite `world_scale`
  - known `asset_type`
  - known `source_mode`
  - finite pivot vectors with non-degenerate local frame axes
- `core_mesh_asset_authoring_document_*` now owns the first file-backed authoring lane:
  - `primitive_seed` object payload arrays
  - shared object/transform/flags metadata per primitive
  - plane primitive payloads
  - rect-prism primitive payloads
  - fixture-compatible save/load for `mesh_asset_authoring_v1`
- `core_mesh_asset_runtime_contract_validate(...)` currently validates only shared runtime semantics:
  - non-empty `asset_id` and `source_asset_id`
  - known `asset_type`
  - positive `vertex_count` and `triangle_count`
  - finite local bounds with `min <= max` on every axis
  - finite pivot vectors with non-degenerate local frame axes

## Status
- Bootstrap contract module for the shared mesh-asset lane.
- First fixtures now cover both the editable `primitive_seed` authoring document lane and the staged runtime/scene-instance shapes ahead of richer mesh-compile integration.
