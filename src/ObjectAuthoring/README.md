# ObjectAuthoring

ObjectAuthoring owns the CAD object-editor document/session contract. It mirrors
the existing primitive object store, records create/sketch/extrude operations,
snapshots extrude add/cut result bodies for replay, and persists the app-local
operation stack inside object asset files. Committed sketch rendering, hitboxes,
and workspace preservation now prefer the active authoring sketch; legacy
`EditorState` sketch fields remain as transient input, preview, and UI
compatibility mirrors.

Phase D stable topology work has its first editable-topology base here.
`ObjectAuthoringFaceId`, `ObjectAuthoringVertexId`, and `ObjectAuthoringEdgeId`
are the app-local stable topology identities for the current
primitive/evaluated model. `ObjectAuthoringFaceRef` carries both the stable
`faceId` and the legacy `Object3DFaceKind` adapter so existing object-mode UI,
render, hitbox, and input paths can keep using primitive face enums while the
authoring model moves toward durable topology refs. Face ids are deterministic
for the current proof: they derive from body id plus primitive face kind.
Vertex and edge ids derive from body id plus primitive topology index.
Authoring documents rebuild evaluated vertices, edges, faces, face-corner
bindings, and edge-to-face adjacency whenever evaluated bodies change.
`ObjectAuthoringSelectionKind` plus vertex/edge refs provide explicit API
targets for viewport topology picking and constrained-gizmo routing. Selected
vertices/edges can now resolve into non-mutating gizmo targets for visible and
clickable local-axis affordances; the next shape-changing boundary is a
semantic topology operation that applies those drags through this authoring
document instead of raw mesh mutation.
`ObjectAuthoringFaceRef_IsSet` and `ObjectAuthoringFaceRef_Matches` are the
operation-facing compatibility helpers: matching prefers stable `faceId` values
when both refs carry them, then falls back to legacy body/primitive matching for
older or adapter-only refs.
`ObjectAuthoringDocument_CheckFaceRef` classifies invalid refs as unset,
missing body, missing face, or stale adapter. Replay now fails sketch/extrude
operations with explicit missing/stale face-ref diagnostics instead of
collapsing those cases into vague body/result errors.

Phase B runtime mesh compile now has its first app-local baseline.
`object_authoring_mesh_compile.*` evaluates the operation stack, emits
deterministic vertices/triangles/bounds, preserves each stable face id as a
runtime surface group (`face_<face_id>`), validates the result, and can write a
`mesh_asset_runtime_v1` JSON file for saved operation-backed object assets.
Object mode now exposes that path through `Export Mesh`: the panel export
entrypoint compiles the attached authoring document, writes
`<asset>.runtime.json` beside the object asset root, and records export
status plus the last runtime mesh path in session state.
The carrier currently uses the vendored shared `CoreMeshAssetRuntimeContract`
plus app-local mesh arrays because the `line_drawing` shared subtree snapshot
does not yet expose the newer `CoreMeshAssetRuntimeDocument` API. Once that
shared module state is committed and rolled into the subtree, the app-local
carrier should collapse onto the shared runtime document type.

The scene workspace stays the top-level arranger. The object workspace attaches
an `ObjectAuthoringSession` to the current isolated asset layout, then uses the
session document as the durable model for future topology, face, sketch,
operation-stack, and node-authoring work.

Current replay support is app-local and primitive-bounded:

- `create_primitive` upserts the captured primitive body snapshot
- `sketch_rect` restores the captured sketch snapshot
- `extrude_add` upserts captured result body snapshots
- `extrude_cut` removes the target body and upserts captured result body
  snapshots

Attached object-mode extrude commits are operation-first: the editor builds the
semantic result snapshot, appends it to the authoring document, evaluates the
document, and then rebuilds `Layout.objectStore` from the evaluated primitive
body list. Direct primitive mutation remains as a compatibility fallback only
when no `ObjectAuthoringSession` is attached.

Object asset persistence now writes the regular primitive-seed compatibility
payload plus a private app-local extension:

- `extensions.line_drawing.object_authoring_v1`

That extension stores schema version, next ids, selected refs, body/sketch
snapshots, operations, and result body snapshots. Loading an asset with the
extension reattaches the authoring document and evaluates it into the isolated
object workspace layout. New face refs save `face_id`; legacy saved refs that
only contain `body_id` plus `primitive_face` derive the same stable face id on
load. Loading a legacy primitive-seed-only asset still seeds equivalent
`create_primitive` operations.

Undo/redo still uses the existing whole-layout JSON snapshot history.
Operation-level undo remains deferred until the topology model grows beyond the
current primitive/evaluated face-id baseline.
