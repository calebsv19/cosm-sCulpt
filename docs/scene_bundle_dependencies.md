# Sculpt scene bundle dependencies

Sculpt owns discovery of external inputs referenced by its retained authoring
model. `core_scene_compile` owns the app-neutral canonical manifest, digest,
publication, and receipt-verification contract.

The File-pane scene exporter currently discovers validated
`mesh_asset_instance` objects. For each distinct mesh asset identity it:

1. resolves the retained runtime-mesh path without mutating the scene;
2. reads and hashes the runtime-mesh bytes;
3. emits one `mesh_asset_runtime` dependency with stable asset identity,
   content SHA-256, and byte count;
4. rejects conflicting content for the same dependency identity;
5. passes the resulting entries to shared canonical sorting and digesting.

The published scene directory contains:

```text
scene_authoring.json
scene_runtime.json
scene_dependencies.json
scene_export_receipt.json
```

Export reports success only after `core_scene_compile_verify_bundle` verifies
the newly published directory against its expected bundle digest.

Current limits:

- the manifest binds mesh content but does not package mesh bytes into the
  scene directory;
- dependency identity-to-payload mapping remains a later worker/package lane;
- texture, fluid-simulation preset, cache, and render-resource discovery must
  be added only when those references have typed retained-scene contracts;
- receipt integrity is not artifact authentication; schedulers should supply
  the externally expected bundle digest retained by their job or handoff.
