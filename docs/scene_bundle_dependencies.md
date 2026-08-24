# Sculpt scene bundle dependencies

Sculpt owns discovery of external inputs referenced by its retained authoring
model. `core_scene_compile` owns the app-neutral canonical manifest, digest,
content-addressed payload publication, and receipt-verification contract.

The File-pane scene exporter currently discovers validated
`mesh_asset_instance` objects. For each distinct mesh asset identity it:

1. resolves the retained runtime-mesh path without mutating the scene, giving
   a scene-owned `assets/mesh_assets/<runtime-basename>` attachment precedence
   over the retained external path and legacy library fallbacks;
2. reads and hashes the runtime-mesh bytes;
3. emits one `mesh_asset_runtime` dependency with stable asset identity,
   content SHA-256, and byte count;
4. rejects conflicting content for the same dependency identity;
5. retains the exact bytes through shared publication;
6. passes the resulting entries to shared canonical sorting and digesting.

The published scene directory contains:

```text
scene_package.json
scene_authoring.json
scene_runtime.json
scene_dependencies.json
scene_export_receipt.json
dependencies/
  mesh_asset_runtime/
    <mesh-content-sha256>
```

`scene_package.json` is the Sculpt-owned stable entrypoint. It declares the
authoring path and the missing, stale, compiled, or verified state of derived
artifacts. For a compiled export, shared `core_scene_compile` stages this file
inside the same atomic transaction, includes its SHA-256 in the bundle digest,
and records its bytes and digest in the receipt. Workers must verify the
receipt and externally expected bundle digest before accepting the package.
For a Save As package, the descriptor exists immediately but derived states
remain `missing` until Export Runtime publishes a separate compiled package.

The canonical `scene_dependency_manifest_v2` path is derived as
`dependencies/<kind>/<sha256>` rather than accepted from authoring input.
Export reports success only after `core_scene_compile_verify_bundle` verifies
the newly published directory, including packaged payload bytes, against its
expected bundle digest. Verification rejects missing files, byte-count or
digest drift, non-regular files, and symlink-backed path components.
Missing or unreadable runtime-mesh diagnostics include the retained scene
object id, asset id, and stored path so an operator can identify the exact
dependency that blocked publication.

On authoring import, Sculpt reconciles each mesh instance in memory against the
same resolved runtime. A matching asset identity may refresh the runtime path,
source identity, bounds, vertex count, and triangle count while preserving the
object transform and lock policy. Missing, unreadable, or identity-mismatched
assets remain unchanged and are reported as unresolved by the reconciliation
API. The loaded source file is never rewritten; corrected metadata is persisted
only by a later explicit Save, Save As, or Export.

Current limits:

- only retained `mesh_asset_runtime` dependencies are packaged today;
- mapping verified mesh bytes into renderer geometry or solver structures
  remains a later consumer-adapter lane;
- texture, fluid-simulation preset, cache, and render-resource discovery must
  be added only when those references have typed retained-scene contracts;
- receipt integrity is not artifact authentication; schedulers should supply
  the externally expected bundle digest retained by their job or handoff.
