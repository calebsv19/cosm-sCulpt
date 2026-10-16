## 2026-10-09 Main Edit progressive mesh previews

Retained Main Edit now prepares four persistent indexed LOD levels in bounded
background jobs, rejects stale scene/source results, culls conservative transformed
object/assembly bounds and selects screen-space detail with hysteresis and a
120k scene triangle plan. Resident lookup is indexed; loading/budget proxies and
separate memory counters are explicit. Source geometry remains authoritative.
See `docs/mesh_preview_cache.md` for limits and native proof. This is uncommitted
Main Edit development, separate from canonical/released 0.6.0. Persistent GPU mesh
instances and a total allocator ceiling remain open.

# Layout Scene

This subtree owns layout-level 3D scene state, scene objects, mesh instances,
and scene helper contracts.

## Ownership

- `layout_scene3d.c` owns scene bounds, construction-plane state, and
  bounds-edit helper APIs.
- `layout_object_store.c` owns the stable scene object store for primitive and
  mesh-instance objects.
- `layout_mesh_asset_instance.c` creates transformable runtime mesh sidecar
  instances and reconciles their cached identity, path, bounds, and topology
  counts against the resolved runtime/preview metadata after scene import.
- `layout_mesh_asset_path_resolver.*` resolves retained mesh references. When
  a scene authoring path is known, its portable
  `assets/mesh_assets/<runtime-basename>` attachment is authoritative before
  the retained external path or legacy library fallbacks.
- `layout_mesh_preview_sidecar.*` and `layout_mesh_runtime_preview.*` own
  bounded mesh preview sidecar loading/render data.
- `layout_mesh_solid_preview.*` owns the LineDrawing-local filled and
  outline-only runtime-mesh preview. It obtains coherent vertex-cluster
  interactive/settled LODs from shared `core_mesh_preview` `0.5.0` through a
  thin adapter over the canonical `core_mesh_asset` document, then rasterizes
  them through a CPU depth
  buffer, adds view-dependent silhouette/depth outlines, and uploads one cached
  viewport texture. Orbit direction changes or scene-geometry changes use an
  8,000-triangle/50%-scale interaction tier; a stable view promotes to the
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
- Resolution changes where Sculpt reads an asset; it does not rewrite retained
  authoring paths or silently replace historical scene attachments.
- Reconciliation is an in-memory load repair. It accepts only matching asset
  identities, preserves object transforms and lock policy, and reaches disk
  only through a later explicit Save, Save As, or Export action.
- The solid/outline preview is an app-local editor visualization, not downstream
  simulation or final RayTracing material authority. Shared code owns only the
  renderer-neutral LOD mesh; native GPU depth rendering, smooth
  normals/material bindings, display policy, and interaction overlays remain
  consumer-owned.

`layout_mesh_preview_cache.*` owns the bounded resident LRU and its invalidation.
`layout_mesh_prepared_store.*` owns private content/version-keyed core_pack disk
entries for reusable indexed LODs and preview bounds. This app cache adds no shared
module API/version changes and is not authoritative export geometry.

## Coupled folding braces

`layout_folding_brace.c` derives fixed-length two-link poses and validates analytic
hinge-range reachability. `layout_folding_brace_json.c` reads/writes the optional
schema-26 constraint record (legacy braces remain readable). Constraint transactions own atomic publication and
history; motion scope lookup includes derived support members. No support state
is independently keyframed. This Main Edit capability does not qualify hardware.

`layout_folding_lock.c` owns explicit manual deployed-pose lock validity and atomic
engage/release. Compact lateral strip/pin and mounting offsets preserve finite
part geometry. Near-vertical rotations use atan2 to retain their horizontal
component. This is kinematics and UI state, not force or natural-collapse simulation.

The articulated inspection lane uses `layout_coverings.c` for metadata-defined
front collections and `layout_guide_groups.c` for nonphysical presentation groups.
`layout_telescoping_stay.c` owns finite rigid coaxial hardware poses, with analytic
whole-range anchor checks in `layout_anchor_span.c` and schema parsing in
`layout_telescoping_stay_json.c`. `layout_motion_interlock.c` validates explicit
closed-neighbour service restrictions before native constraint publication.


Scene collections are implemented in `layout_scene_collections.c` and its JSON companion. They own durable semantic membership and working-view metadata independently of geometry and assembly motion. See `docs/scene_collections.md`.


Hot visibility paths must use resolved entity metadata when available. `Layout_CoveringInfoContains` supports queries and row evaluation without re-searching the object store. Do not call whole-collection validation inside each-object visibility loops. The optional native performance route in `Core/scene_collections_proof.c` compares full-scene closed/open panel, true orbit, and covering toggles.

Pass 1 visibility policy lives in `layout_visibility_actions.c`: scoped Reveal and
blocker explanations reuse native collection membership. Resolved object visibility
avoids redundant entity lookup; camera signatures retain only filled-surface inputs.

Collection dimensional evaluation lives in layout_collection_inspection.c. Saved read-only records and JSON live in layout_saved_measurements.c / layout_saved_measurements_json.c. Metadata publishes through a dedicated history-aware transaction; no constraint solver or preview geometry is driven.


layout_construction_reference.c builds read-only current-pose construction specifications; layout_reference_sheets.c renders escaped print sheets from that specification. Existing core math/units and inspection corners are reused; app-specific fabrication review policy stays local.

## Preview shutdown qualification — 2026-10-15

See Main Edit `docs/mesh_preview_shutdown.md`: bounded app-local input readers
retain existing shared workers and source validation. Four focused cases and
637 cases / 70 groups pass. Three native restored-workspace quit checks preserve
scene/history/draft state and finish shutdown in 14–16 ms. The previous acceptance
blocker is resolved in the private package; adoption/Desktop refresh remain next.
