# Agent Scene Authoring CLI

Last updated: 2026-05-20

`agent_scene_tool` is the first headless authoring entrypoint for LineDrawing. It converts a compact, agent-authored JSON request into the existing LineDrawing scene pipeline outputs without opening the SDL app.

## Build

```bash
make -C line_drawing agent_scene_tool
```

## Run

```bash
./line_drawing/build/toolchains/clang/bin/agent_scene_tool \
  --request line_drawing/tests/fixtures/agent_room_prisms_request.json \
  --out line_drawing/tmp/agent_room_prisms \
  --determinism-check
```

## Optional Refinement Pass

For room-style review scenes, run the deterministic refinement helper before
compile when you want better default staging for camera placement, tall-prism
spacing, and light-path clearance:

```bash
python3 line_drawing/tools/agent_scene_refine.py \
  --request <request.json> \
  --output <refined_request.json> \
  --report <refine_report.json>
```

Current refinement behavior:

- moves the authored camera to the room corner opposite the wall intersection
- places authored camera-path points outside the floor footprint by default for
  room-style scenes
- disables `bounds.clamp_on_edit` on the refined request so camera-path edits
  are not forced back inside authored scene bounds
- writes authored `camera_path.rotation` and `camera_path_depth.lookPitch`
  from the same focus-target math used by the runtime camera sampler so editor
  vectors and startup previews agree with the detached render lane
- computes `extensions.ray_tracing.authoring.camera_focus_target` from the
  transparent prism cluster
- nudges tall transparent prisms apart when their footprints crowd each other
- replaces the authored light path with a safer perimeter route around the
  prism cluster and reports sampled collisions before/after

Use the refined request as the input to `agent_scene_tool`. Treat the report as
an agent-facing diagnostic artifact, not a source-of-truth scene file.

The output directory contains:

- `agent_request.json`: copied input request.
- `layout.json`: generated LineDrawing layout.
- `scene_authoring.json`: canonical `scene_authoring_v1` export.
- `scene_runtime.json`: compiled `scene_runtime_v1` payload.
- `scene_summary.json`: machine-readable scene summary and file index.

When `--out` is `<run_dir>/line_drawing`, the tool also writes app-loadable
convenience outputs beside that directory:

- `<run_dir>/<scene_slug>.layout.json`: load through LineDrawing `Load JSON`.
- `<run_dir>/line_drawing_app_load/scene_authoring.json`: load through
  LineDrawing `Load Scene`.
- `<run_dir>/line_drawing_app_load/scene_runtime.json`: paired runtime scene
  contract file for `Load Scene` discovery.

`scene_runtime.json` is compiled output. Do not select it directly in
LineDrawing; use `Load Scene` on a folder that directly contains both
`scene_authoring.json` and `scene_runtime.json`.

## Request Schema

The initial schema is `line_drawing_agent_scene_request_v1`.

Required top-level fields:

- `schema`: must be `line_drawing_agent_scene_request_v1`.
- `scene_id`: stable scene identifier.
- `objects`: array of `plane` and `rect_prism` entries.

Optional top-level fields:

- `grid_size`: LineDrawing layout grid size, default `1.0`.
- `bounds`: scene bounds with `enabled`, `clamp_on_edit`, `min`, and `max`.
- `construction_plane`: default authoring plane with `axis` (`xy`, `yz`, `xz`) and `offset`.
- `scene_options`: canonical scene material, light, and camera IDs/types passed through the existing LineDrawing scene exporter.
- `extensions`: namespace passthrough copied into generated scene `extensions`.
- `physics_sim`: shortcut for generated `extensions.physics_sim`; supports `scene_domain` and `object_overlays`.

Object fields:

- `id`: stable scene object id. This becomes the canonical scene `object_id`.
- `kind`: `plane` or `rect_prism`.
- `axis`: primitive frame axis, one of `xy`, `yz`, `xz`.
- `position`: object center `{ "x": n, "y": n, "z": n }`.
- `width`, `height`: plane or prism frame extents.
- `depth`: rect-prism normal-axis extent.
- `lock_to_construction_plane`: defaults to `false` for agent-authored free placement.
- `lock_to_bounds`: defaults to `true`.
- `physics_sim`: object-local shortcut appended to `extensions.physics_sim.object_overlays` with this object's `id`.

PhysicsSim scene domain example:

```json
"physics_sim": {
  "scene_domain": {
    "active": true,
    "shape": "box",
    "min": { "x": -3.0, "y": -3.0, "z": -0.25 },
    "max": { "x": 3.0, "y": 3.0, "z": 2.5 }
  }
}
```

PhysicsSim object emitter example:

```json
{
  "id": "emitter_prism",
  "kind": "rect_prism",
  "axis": "xy",
  "position": { "x": 0.0, "y": 0.0, "z": 0.2 },
  "width": 0.7,
  "height": 0.7,
  "depth": 0.4,
  "physics_sim": {
    "motion_mode": "Dynamic",
    "emitter": {
      "active": true,
      "type": "Density",
      "radius": 0.45,
      "strength": 80.0,
      "direction": { "x": 0.0, "y": 0.0, "z": 1.0 }
    }
  }
}
```

`extensions.ray_tracing` is a passthrough namespace for trio bridge metadata.
The current detached review lane uses:

- `authoring.light_path`
- `authoring.light_path_depth`
- `authoring.camera_path`
- `authoring.camera_path_depth`
- `authoring.object_materials`

`object_materials` entries map authored object ids to RayTracing review-only
material/color overlays such as:

```json
{
  "object_id": "wide_block",
  "material_id": 0,
  "object_color": 6707547,
  "alpha": 0.85,
  "reflectivity": 0.35,
  "roughness": 0.45,
  "emissive_strength": 0.0
}
```

Supported per-object RayTracing overlay fields are:

- `material_id`
- `object_color`
- `alpha` (alias `transparency` also accepted downstream)
- `reflectivity`
- `roughness`
- `emissive_strength`

Notes:

- Omit `emissive_strength` for normal objects; downstream runtime defaults it to `0.0`.
- Use RayTracing `material_id = 5` for transparent glass-like objects.
- Use RayTracing `material_id = 4` only for intentionally emissive objects.

The first object-wide procedural texture shorthand fields are also supported on
the same `object_materials` entries:

- `texture_id`
- `texture_strength`
- `texture_scale`
- `texture_offset_u`
- `texture_offset_v`
- `texture_seed`
- `texture_pattern_mode`
- `texture_coverage`
- `texture_grain`
- `texture_edge_softness`
- `texture_contrast`
- `texture_flow`
- `texture_color_depth`
- `texture_surface_damage`

For richer texture authoring, the downstream RayTracing bridge still accepts a
nested `procedural_texture` object. Use the flat shorthand when you just need a
quick object-wide surface treatment from an agent-authored scene.

These overlays let agent-authored scenes carry readable detached-review
material intent without changing the core LineDrawing scene export contract.
Use them for tinted glass, rough/dull vs glossy surfaces, and stronger emissive
review cues while keeping the underlying authored geometry stable.

## Verification

```bash
make -C line_drawing agent-scene-smoke
make -C line_drawing scene-pipeline-smoke
./bin/run_trio_scene_pipeline.sh --authoring line_drawing/tmp/agent_room_prisms/scene_authoring.json --skip-export
```

For the detached trio handoff lane, use:

```bash
./bin/run_trio_detached_job_chain.sh submit \
  --line-request line_drawing/tests/fixtures/agent_detached_gallery_emitter_paths_request.json \
  --profile review
./bin/run_trio_detached_job_chain.sh status \
  --run-root _private_workspace_artifacts/agent_runs/physics_trio/agent_detached_gallery_emitter_paths_request_detached_chain
```

That path keeps LineDrawing authoring synchronous, then stages detached
PhysicsSim and RayTracing jobs through their file-backed job runners.

Named detached profiles:

- `preview`: fast contract check, low cost, short heartbeat cadence.
- `review`: balanced default for normal interactive detached review runs.
- `long_review`: deeper multi-frame review with slower follow-ups.
- `overnight`: intended for late-night runs; metadata recommends a deferred
  first check (default `06:00`) and slower hourly-style follow-ups.

The chain root now also writes:

- `chain_status.json`: current live state.
- `chain_summary.json`: one top-level manifest with profile metadata, child job
  ids, requested work, monitoring guidance, and artifact roots.

`chain_summary.json` monitoring guidance is now explicit enough for Codex-side
automation setup:

- `runtime_class`
- `heartbeat_interval_minutes`
- `followup_interval_minutes`
- `defer_first_check_until_local`
- `automation_recommendation.kind`
- `automation_recommendation.initial_check_local_time`
- `automation_recommendation.recurring_interval_minutes`

The detached gallery emitter fixture now includes
`extensions.ray_tracing.authoring.object_materials` overlays so first-pass
review scenes have contrast objects and an emissive-tinted emitter instead of
all-white geometry.

Available detached review fixtures now include:

- [agent_detached_gallery_emitter_paths_request.json](/Users/calebsv/Desktop/CodeWork/line_drawing/tests/fixtures/agent_detached_gallery_emitter_paths_request.json)
- [agent_detached_lab_emitter_paths_request.json](/Users/calebsv/Desktop/CodeWork/line_drawing/tests/fixtures/agent_detached_lab_emitter_paths_request.json)

Use `gallery_contrast` when you want a room/gallery read with simple plinths and
columns. Use `lab_contrast` when you want a denser equipment-like arrangement
with screens, baffles, and a taller emitter column.
