#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
LINE_DIR="$ROOT/line_drawing"
LD_CLI_SMOKE_LABEL="line-imported-mesh-harness"
source "$LINE_DIR/tests/lib/cli_smoke_helpers.sh"
TOOL_REL="$(make -s -C "$LINE_DIR" print-imported-mesh-harness-bin)"
TOOL="$LINE_DIR/$TOOL_REL"

if [[ -z "$TOOL_REL" || ! -x "$TOOL" ]]; then
  ld_cli_smoke_error "harness binary not found at $TOOL"
  exit 1
fi

FAIL_DIR="$LINE_DIR/tmp/imported_mesh_harness_failure_r4"
ld_cli_smoke_reset_dir "$FAIL_DIR"

ld_cli_smoke_expect_failure "missing-args" "$FAIL_DIR/missing_args.err" "$TOOL"

ld_cli_smoke_require_pattern 'usage:' "$FAIL_DIR/missing_args.err"

ld_cli_smoke_expect_failure "missing-STL" \
  "$FAIL_DIR/missing_stl.err" \
  "$TOOL" \
  --stl "$FAIL_DIR/missing.stl" \
  --out "$FAIL_DIR/out_missing" \
  --asset-id "asset_missing_stl" \
  --scene-id "scene_missing_stl" \
  --object-id "object_missing_stl"

ld_cli_smoke_require_pattern 'imported_mesh_harness: failed to prepare output paths' "$FAIL_DIR/missing_stl.err"

printf 'not an stl\n' > "$FAIL_DIR/invalid.stl"
ld_cli_smoke_expect_failure "invalid-STL" \
  "$FAIL_DIR/invalid_stl.err" \
  "$TOOL" \
  --stl "$FAIL_DIR/invalid.stl" \
  --out "$FAIL_DIR/out_invalid" \
  --asset-id "asset_invalid_stl" \
  --scene-id "scene_invalid_stl" \
  --object-id "object_invalid_stl"

ld_cli_smoke_require_pattern 'imported_mesh_harness: runtime compile failed:' "$FAIL_DIR/invalid_stl.err"

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

  ld_cli_smoke_require_file "$authoring"
  ld_cli_smoke_require_file "$runtime"
  ld_cli_smoke_require_file "$scene"
  ld_cli_smoke_require_file "$summary"

  ld_cli_smoke_require_pattern '"schema_variant"[[:space:]]*:[[:space:]]*"mesh_asset_authoring_v1"' "$authoring"
  ld_cli_smoke_require_pattern '"source_mode"[[:space:]]*:[[:space:]]*"imported_mesh"' "$authoring"
  ld_cli_smoke_require_pattern '"source_format"[[:space:]]*:[[:space:]]*"stl"' "$authoring"
  ld_cli_smoke_require_pattern '"default_surface_group_id"[[:space:]]*:[[:space:]]*"imported_surface"' "$authoring"
  ld_cli_smoke_require_pattern '"schema_variant"[[:space:]]*:[[:space:]]*"mesh_asset_runtime_v1"' "$runtime"
  ld_cli_smoke_require_pattern "\"vertex_count\"[[:space:]]*:[[:space:]]*$expected_vertices" "$runtime"
  ld_cli_smoke_require_pattern "\"triangle_count\"[[:space:]]*:[[:space:]]*$expected_triangles" "$runtime"
  ld_cli_smoke_require_pattern '"group_id"[[:space:]]*:[[:space:]]*"imported_surface"' "$runtime"
  ld_cli_smoke_require_pattern '"schema_variant"[[:space:]]*:[[:space:]]*"scene_runtime_v1"' "$scene"
  ld_cli_smoke_require_pattern '"object_type"[[:space:]]*:[[:space:]]*"mesh_asset_instance"' "$scene"
  ld_cli_smoke_require_pattern '"kind"[[:space:]]*:[[:space:]]*"mesh_asset"' "$scene"
  ld_cli_smoke_require_pattern "\"id\"[[:space:]]*:[[:space:]]*\"$asset_id\"" "$scene"
  ld_cli_smoke_require_pattern '"schema"[[:space:]]*:[[:space:]]*"line_drawing_imported_mesh_harness_summary_v1"' "$summary"
  ld_cli_smoke_require_pattern "\"vertices\"[[:space:]]*:[[:space:]]*$expected_vertices" "$summary"
  ld_cli_smoke_require_pattern "\"triangles\"[[:space:]]*:[[:space:]]*$expected_triangles" "$summary"
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
