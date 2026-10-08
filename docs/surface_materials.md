# Per-object render finishes

Select an object in the Layout workspace and use **Object > Finish** to cycle
through Default, Wood grain, Painted shell, Bare brushed metal, Fabric, Foam,
Rubber and Plastic. Save the layout normally. Finish edits participate in undo
and redo. They are independent of construction-material properties, dimensions,
physical density and assembly membership, and do not consume property slots.

The native entity record stores the optional `surfaceMaterial` preset ID. Old
layouts default to no override. Unknown explicit IDs are rejected rather than
silently substituted. Material preview uses the preset color; procedural texture
and reflection are evaluated by optiC. This preview does not claim shader parity.

The normal **Export scene** route writes a compiled scene bundle with per-object
material references and bindings. Existing optiC `object_materials` fields carry
color, roughness, reflectivity and the procedural `wood` or `brushed_metal` stack.
Painted shell has a subdued finish rather than bare-metal reflectivity. These are
appearance presets, not measured optical properties or a conductor-BSDF claim.
Existing typed shader graphs are not silently overwritten by a preset export.
Spatial planning volumes remain in the saved layout but are omitted from opaque
render geometry. Mesh dependencies, including panel openings, are packaged and
bound to contained content-addressed paths.

For deterministic native batch edits:

```sh
make surface_material_tool
# New output only; a bad row rejects the entire unpublished candidate.
build/toolchains/clang/bin/surface_material_tool apply input.layout.json assignments.json NEW.layout.json
```

Assignments use stable object IDs:

```json
{
  "schema": "line_drawing_surface_materials_v1",
  "assignments": [
    {"object_id": "bed_deck", "preset": "wood"},
    {"object_id": "oem_floor", "preset": "painted_metal"}
  ]
}
```

The accepted IDs are `wood`, `painted_metal`, `brushed_metal`, `fabric`, `foam`,
`rubber` and `plastic`. Empty `preset` removes the override. Duplicate IDs,
unknown IDs, assemblies and spatial guides are refused. The input and existing
output files are preserved.

## Camera handoff

The [camera export tool](camera_view.md) now publishes a verified scene bundle,
including its dependency manifest and payloads. Its `camera_samples.json` remains
the source for the full orientation, timing and clipping values.

```sh
python3 tools/export_ray_camera_requests.py SCENE_EXPORT NEW_REQUEST_DIR
python3 tools/export_ray_camera_requests.py SCENE_EXPORT NEW_FRAME_REQUEST_DIR --samples frames
```

This writes ordinary `ray_tracing_agent_render_request_v1` still requests with
relative scene paths and separate output directories. Each request uses the
sample's position and look direction; the existing optiC zoom control translates
vertical FOV using its 55-degree base projection. Rolled or vertical camera frames
are refused before any output is created. Near/far overrides are unavailable in
that request API and remain explicitly unqualified. Ambient inspection fill is
provisional. The command creates requests; it does not submit jobs, render a
video, or modify RayTracing.

Wood exports use procedural placement scale 8 for finer grain at van dimensions;
brushed metal retains scale 1. This controls appearance, not measured timber grain.
