# Tools

This lane owns reusable export, import, diagnostics, and agent-authoring tools
that either link into the runtime or build as focused command-line helpers.

## Source Lanes

- `ShapeLib/` — pure shape structs, flattening, drawing, and JSON IO used by
  the Layout-to-Shape export path.
- `shape_from_layout.*`, `shape_export.*`, `shape_tool.c`,
  `shape_pack_tool.c`, `shape_trace_tool.c`, `shape_sanity_tool.c`, and
  `shape_dataset.*` — Shape export, diagnostics pack, trace, sanity, and
  dataset helpers.
- `scene_export.*` — scene-directory export helper used by the File pane. It
  writes authored scenes and compiles runtime scenes through shared
  `core_scene_compile`.
- `scene_project_export.*` — scene-project metadata and scaffold helper used by
  `scene_export.*` for project-root exports. It writes `scene_project.json`,
  `object_manifest.json`, downstream placeholder folders, and project-local
  runtime mesh sidecar copies for manifest mesh objects.
- `scene_import.*` and `scene_authoring_import.*` — authored-scene import and
  round-trip restore helpers. Canonical top-level camera/light/path/material
  records reconstruct editable LineDrawing authoring state even without an
  embedded layout snapshot; when a snapshot exists, portable canonical records
  overlay it and bindings are normalized.
- `canonical_scene_export.*` and `canonical_scene_export_primitives.*` —
  canonical scene authoring/export seams and current compiler-unit dump
  customers for scene metadata and primitive payload contracts.
- `imported_mesh_harness.c` — deterministic imported mesh/STL harness that
  proves shared `core_mesh_asset` and `core_mesh_compile` integration without
  making the app UI own parser policy.
- `agent_scene_tool.c` and `agent_scene_material_flow.*` — agent-authored scene
  request compilation, mesh asset instance request support, and RayTracing
  material-intent mapping.
- `global_state_stub.c` — test/tool support for linking tool paths that need a
  bounded `GlobalState` seam.

## Boundary

- Tooling may share pure conversion/export helpers with the runtime, but it
  should not own interactive editor policy.
- STL/imported mesh parser behavior belongs to the shared mesh libraries and
  the bounded import adapter/harness paths; richer app UI belongs to the
  File-pane lane.
- `canonical_scene_export.c` and `agent_scene_tool.c` are future R5 or
  decomposition candidates because they are over the warning threshold. Do not
  extract them during R0 unless a later implementation slice explicitly chooses
  that boundary and runs parity tests.
- `agent_scene_tool.c` and `imported_mesh_harness.c` currently keep local CLI
  path helpers because request-relative paths, output roots, and artifact copy
  behavior are command-specific. Treat any shared CLI helper extraction as a
  later R5/testability slice, not as part of R1 test fixture cleanup.
- CLI diagnostic coverage lives in the existing shell smoke scripts. Keep
  command-specific failure messages stable enough for those tests, but do not
  extract shared CLI argument/path helpers until a later R5 slice chooses that
  boundary.
