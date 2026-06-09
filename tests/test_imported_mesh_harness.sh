#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
LINE_DIR="$ROOT/line_drawing"
TOOL_REL="$(make -s -C "$LINE_DIR" print-imported-mesh-harness-bin)"
TOOL="$LINE_DIR/$TOOL_REL"

if [[ -z "$TOOL_REL" || ! -x "$TOOL" ]]; then
  echo "[line-imported-mesh-harness] ERROR: harness binary not found at $TOOL" >&2
  exit 1
fi

require_pattern() {
  local pattern="$1"
  local path="$2"
  if ! rg -q "$pattern" "$path"; then
    echo "[line-imported-mesh-harness] ERROR: expected pattern '$pattern' in $path" >&2
    exit 1
  fi
}

run_import_case() {
  local name="$1"
  local stl="$2"
  local asset_id="$3"
  local expected_vertices="$4"
  local expected_triangles="$5"
  local out_dir="$LINE_DIR/tmp/imported_mesh_harness_$name"
  local authoring="$out_dir/authoring/$asset_id.authoring.json"
  local runtime="$out_dir/assets/mesh_assets/$asset_id.runtime.json"
  local scene="$out_dir/scene_runtime.json"
  local summary="$out_dir/import_summary.json"

  rm -rf "$out_dir"
  "$TOOL" \
    --stl "$stl" \
    --out "$out_dir" \
    --asset-id "$asset_id" \
    --scene-id "scene_line_drawing_imported_${name}_harness" \
    --object-id "obj_imported_${name}_harness" >/dev/null

  test -f "$authoring"
  test -f "$runtime"
  test -f "$scene"
  test -f "$summary"

  require_pattern '"schema_variant"[[:space:]]*:[[:space:]]*"mesh_asset_authoring_v1"' "$authoring"
  require_pattern '"source_mode"[[:space:]]*:[[:space:]]*"imported_mesh"' "$authoring"
  require_pattern '"source_format"[[:space:]]*:[[:space:]]*"stl"' "$authoring"
  require_pattern '"default_surface_group_id"[[:space:]]*:[[:space:]]*"imported_surface"' "$authoring"
  require_pattern '"schema_variant"[[:space:]]*:[[:space:]]*"mesh_asset_runtime_v1"' "$runtime"
  require_pattern "\"vertex_count\"[[:space:]]*:[[:space:]]*$expected_vertices" "$runtime"
  require_pattern "\"triangle_count\"[[:space:]]*:[[:space:]]*$expected_triangles" "$runtime"
  require_pattern '"group_id"[[:space:]]*:[[:space:]]*"imported_surface"' "$runtime"
  require_pattern '"schema_variant"[[:space:]]*:[[:space:]]*"scene_runtime_v1"' "$scene"
  require_pattern '"object_type"[[:space:]]*:[[:space:]]*"mesh_asset_instance"' "$scene"
  require_pattern '"kind"[[:space:]]*:[[:space:]]*"mesh_asset"' "$scene"
  require_pattern "\"id\"[[:space:]]*:[[:space:]]*\"$asset_id\"" "$scene"
  require_pattern '"schema"[[:space:]]*:[[:space:]]*"line_drawing_imported_mesh_harness_summary_v1"' "$summary"
  require_pattern "\"vertices\"[[:space:]]*:[[:space:]]*$expected_vertices" "$summary"
  require_pattern "\"triangles\"[[:space:]]*:[[:space:]]*$expected_triangles" "$summary"
}

run_import_case \
  "tetrahedron" \
  "$LINE_DIR/third_party/codework_shared/core/core_mesh_compile/tests/fixtures/imports/tetrahedron_ascii.stl" \
  "asset_imported_tetrahedron_line_harness" \
  4 \
  4

run_import_case \
  "stepped_column" \
  "$LINE_DIR/tests/fixtures/imports/stepped_column_ascii.stl" \
  "asset_imported_stepped_column_line_harness" \
  16 \
  24

echo "[line-imported-mesh-harness] PASS"
