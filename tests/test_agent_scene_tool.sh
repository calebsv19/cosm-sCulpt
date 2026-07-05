#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
LINE_DIR="$ROOT/line_drawing"
LD_CLI_SMOKE_LABEL="line-agent-scene"
source "$LINE_DIR/tests/lib/cli_smoke_helpers.sh"
OUT_DIR="$LINE_DIR/tmp/agent_room_prisms"
REQUEST="$LINE_DIR/tests/fixtures/agent_room_prisms_request.json"
GALLERY_OUT_DIR="$LINE_DIR/tmp/agent_gallery_room_blocks_v2/line_drawing"
GALLERY_REQUEST="$LINE_DIR/tests/fixtures/agent_gallery_room_blocks_v2_request.json"
EMITTER_OUT_DIR="$LINE_DIR/tmp/agent_floor_prism_emitter/line_drawing"
EMITTER_REQUEST="$LINE_DIR/tests/fixtures/agent_floor_prism_emitter_request.json"
MESH_OUT_DIR="$LINE_DIR/tmp/agent_low_poly_mesh_sphere/line_drawing"
MESH_REQUEST="$LINE_DIR/tests/fixtures/agent_low_poly_mesh_sphere_request.json"
IMPORTED_MATERIAL_OUT_DIR="$LINE_DIR/tmp/agent_imported_mesh_material_prompt/line_drawing"
IMPORTED_MATERIAL_REQUEST="$LINE_DIR/tests/fixtures/agent_imported_mesh_material_prompt_request.json"
TOOL_REL="$(make -s -C "$LINE_DIR" print-agent-scene-tool-bin)"
TOOL="$LINE_DIR/$TOOL_REL"
MODE="${1:-all}"

if [[ -z "$TOOL_REL" || ! -x "$TOOL" ]]; then
  ld_cli_smoke_error "agent_scene_tool binary not found at $TOOL"
  exit 1
fi

run_failure_cases() {
FAIL_DIR="$LINE_DIR/tmp/agent_scene_tool_failure_r4"
ld_cli_smoke_reset_dir "$FAIL_DIR"

ld_cli_smoke_expect_failure "missing-args" "$FAIL_DIR/missing_args.err" "$TOOL"

ld_cli_smoke_require_pattern 'usage:' "$FAIL_DIR/missing_args.err"

ld_cli_smoke_expect_failure "missing-request" \
  "$FAIL_DIR/missing_request.err" \
  "$TOOL" --request "$FAIL_DIR/missing_request.json" --out "$FAIL_DIR/out"

ld_cli_smoke_require_pattern '\[agent_scene_tool\] ERROR: failed to read request' "$FAIL_DIR/missing_request.err"

printf '{"schema":"wrong_schema"}\n' > "$FAIL_DIR/bad_schema.json"
ld_cli_smoke_expect_failure "bad-schema" \
  "$FAIL_DIR/bad_schema.err" \
  "$TOOL" --request "$FAIL_DIR/bad_schema.json" --out "$FAIL_DIR/out_bad_schema"

ld_cli_smoke_require_pattern '\[agent_scene_tool\] ERROR: unsupported request schema' "$FAIL_DIR/bad_schema.err"
}

run_success_cases() {
rm -rf "$OUT_DIR"
"$TOOL" --request "$REQUEST" --out "$OUT_DIR" --determinism-check >/dev/null

ld_cli_smoke_require_pattern '"schema"[[:space:]]*:[[:space:]]*"line_drawing_agent_scene_request_v1"' "$OUT_DIR/agent_request.json"
ld_cli_smoke_require_pattern '"objectType"[[:space:]]*:[[:space:]]*"plane_primitive"' "$OUT_DIR/layout.json"
ld_cli_smoke_require_pattern '"objectType"[[:space:]]*:[[:space:]]*"rect_prism_primitive"' "$OUT_DIR/layout.json"
ld_cli_smoke_require_pattern '"schema_variant"[[:space:]]*:[[:space:]]*"scene_authoring_v1"' "$OUT_DIR/scene_authoring.json"
ld_cli_smoke_require_pattern '"schema_variant"[[:space:]]*:[[:space:]]*"scene_runtime_v1"' "$OUT_DIR/scene_runtime.json"
ld_cli_smoke_require_pattern '"object_type"[[:space:]]*:[[:space:]]*"plane_primitive"' "$OUT_DIR/scene_runtime.json"
ld_cli_smoke_require_pattern '"object_type"[[:space:]]*:[[:space:]]*"rect_prism_primitive"' "$OUT_DIR/scene_runtime.json"
ld_cli_smoke_require_pattern '"app_layout"[[:space:]]*:[[:space:]]*"' "$OUT_DIR/scene_summary.json"
ld_cli_smoke_require_pattern '"app_authoring"[[:space:]]*:[[:space:]]*"' "$OUT_DIR/scene_summary.json"
ld_cli_smoke_require_pattern '"app_runtime"[[:space:]]*:[[:space:]]*"' "$OUT_DIR/scene_summary.json"
ld_cli_smoke_require_pattern '"planes"[[:space:]]*:[[:space:]]*3' "$OUT_DIR/scene_summary.json"
ld_cli_smoke_require_pattern '"rect_prisms"[[:space:]]*:[[:space:]]*3' "$OUT_DIR/scene_summary.json"
ld_cli_smoke_require_file "$OUT_DIR/scene_agent_room_prisms.layout.json"
ld_cli_smoke_require_file "$OUT_DIR/line_drawing_app_load/scene_authoring.json"
ld_cli_smoke_require_file "$OUT_DIR/line_drawing_app_load/scene_runtime.json"

rm -rf "$LINE_DIR/tmp/agent_gallery_room_blocks_v2"
"$TOOL" --request "$GALLERY_REQUEST" --out "$GALLERY_OUT_DIR" --determinism-check >/dev/null

ld_cli_smoke_require_pattern '"planes"[[:space:]]*:[[:space:]]*1' "$GALLERY_OUT_DIR/scene_summary.json"
ld_cli_smoke_require_pattern '"rect_prisms"[[:space:]]*:[[:space:]]*6' "$GALLERY_OUT_DIR/scene_summary.json"
ld_cli_smoke_require_pattern '"bounds_adjusted"[[:space:]]*:[[:space:]]*0' "$GALLERY_OUT_DIR/scene_summary.json"
ld_cli_smoke_require_file "$LINE_DIR/tmp/agent_gallery_room_blocks_v2/agent_gallery_room_blocks_v2.layout.json"
ld_cli_smoke_require_file "$LINE_DIR/tmp/agent_gallery_room_blocks_v2/line_drawing_app_load/scene_authoring.json"
ld_cli_smoke_require_file "$LINE_DIR/tmp/agent_gallery_room_blocks_v2/line_drawing_app_load/scene_runtime.json"

rm -rf "$LINE_DIR/tmp/agent_floor_prism_emitter"
"$TOOL" --request "$EMITTER_REQUEST" --out "$EMITTER_OUT_DIR" --determinism-check >/dev/null

ld_cli_smoke_require_pattern '"object_id"[[:space:]]*:[[:space:]]*"floor"' "$EMITTER_OUT_DIR/scene_runtime.json"
ld_cli_smoke_require_pattern '"object_id"[[:space:]]*:[[:space:]]*"emitter_prism"' "$EMITTER_OUT_DIR/scene_runtime.json"
ld_cli_smoke_require_pattern '"physics_sim"[[:space:]]*:' "$EMITTER_OUT_DIR/scene_runtime.json"
ld_cli_smoke_require_pattern '"scene_domain"[[:space:]]*:' "$EMITTER_OUT_DIR/scene_runtime.json"
ld_cli_smoke_require_pattern '"object_overlays"[[:space:]]*:' "$EMITTER_OUT_DIR/scene_runtime.json"
ld_cli_smoke_require_pattern '"motion_mode"[[:space:]]*:[[:space:]]*"Static"' "$EMITTER_OUT_DIR/scene_runtime.json"
ld_cli_smoke_require_pattern '"motion_mode"[[:space:]]*:[[:space:]]*"Dynamic"' "$EMITTER_OUT_DIR/scene_runtime.json"
ld_cli_smoke_require_pattern '"emitter"[[:space:]]*:' "$EMITTER_OUT_DIR/scene_runtime.json"
ld_cli_smoke_require_pattern '"strength"[[:space:]]*:[[:space:]]*80' "$EMITTER_OUT_DIR/scene_runtime.json"
ld_cli_smoke_require_file "$LINE_DIR/tmp/agent_floor_prism_emitter/agent_floor_prism_emitter.layout.json"
ld_cli_smoke_require_file "$LINE_DIR/tmp/agent_floor_prism_emitter/line_drawing_app_load/scene_authoring.json"
ld_cli_smoke_require_file "$LINE_DIR/tmp/agent_floor_prism_emitter/line_drawing_app_load/scene_runtime.json"

rm -rf "$LINE_DIR/tmp/agent_low_poly_mesh_sphere"
"$TOOL" --request "$MESH_REQUEST" --out "$MESH_OUT_DIR" --determinism-check >/dev/null

ld_cli_smoke_require_pattern '"object_id"[[:space:]]*:[[:space:]]*"low_poly_sphere"' "$MESH_OUT_DIR/scene_authoring.json"
ld_cli_smoke_require_pattern '"object_type"[[:space:]]*:[[:space:]]*"mesh_asset_instance"' "$MESH_OUT_DIR/scene_authoring.json"
ld_cli_smoke_require_pattern '"geometry_ref"[[:space:]]*:' "$MESH_OUT_DIR/scene_authoring.json"
ld_cli_smoke_require_pattern '"kind"[[:space:]]*:[[:space:]]*"mesh_asset"' "$MESH_OUT_DIR/scene_authoring.json"
ld_cli_smoke_require_pattern '"id"[[:space:]]*:[[:space:]]*"asset_sphere_8x4"' "$MESH_OUT_DIR/scene_authoring.json"
ld_cli_smoke_require_pattern '"material_id"[[:space:]]*:[[:space:]]*"mat_low_poly_sphere"' "$MESH_OUT_DIR/scene_authoring.json"
ld_cli_smoke_require_pattern '"object_type"[[:space:]]*:[[:space:]]*"mesh_asset_instance"' "$MESH_OUT_DIR/scene_runtime.json"
ld_cli_smoke_require_pattern '"mesh_asset_instances"[[:space:]]*:[[:space:]]*1' "$MESH_OUT_DIR/scene_summary.json"
ld_cli_smoke_require_pattern '"mesh_assets_copied"[[:space:]]*:[[:space:]]*1' "$MESH_OUT_DIR/scene_summary.json"
ld_cli_smoke_require_file "$MESH_OUT_DIR/assets/mesh_assets/asset_sphere_8x4.runtime.json"
ld_cli_smoke_require_file "$LINE_DIR/tmp/agent_low_poly_mesh_sphere/line_drawing_app_load/assets/mesh_assets/asset_sphere_8x4.runtime.json"
ld_cli_smoke_require_file "$LINE_DIR/tmp/agent_low_poly_mesh_sphere/agent_low_poly_mesh_sphere.layout.json"
ld_cli_smoke_require_file "$LINE_DIR/tmp/agent_low_poly_mesh_sphere/line_drawing_app_load/scene_authoring.json"
ld_cli_smoke_require_file "$LINE_DIR/tmp/agent_low_poly_mesh_sphere/line_drawing_app_load/scene_runtime.json"

rm -rf "$LINE_DIR/tmp/agent_imported_mesh_material_prompt"
"$TOOL" --request "$IMPORTED_MATERIAL_REQUEST" --out "$IMPORTED_MATERIAL_OUT_DIR" --determinism-check >/dev/null

ld_cli_smoke_require_pattern '"object_id"[[:space:]]*:[[:space:]]*"imported_skull_proxy"' "$IMPORTED_MATERIAL_OUT_DIR/scene_runtime.json"
ld_cli_smoke_require_pattern '"object_type"[[:space:]]*:[[:space:]]*"mesh_asset_instance"' "$IMPORTED_MATERIAL_OUT_DIR/scene_runtime.json"
ld_cli_smoke_require_pattern '"object_materials"[[:space:]]*:' "$IMPORTED_MATERIAL_OUT_DIR/scene_runtime.json"
ld_cli_smoke_require_pattern '"material_texture_stack"[[:space:]]*:' "$IMPORTED_MATERIAL_OUT_DIR/scene_runtime.json"
ld_cli_smoke_require_pattern '"kind"[[:space:]]*:[[:space:]]*"brushed_metal"' "$IMPORTED_MATERIAL_OUT_DIR/scene_runtime.json"
ld_cli_smoke_require_pattern '"kind"[[:space:]]*:[[:space:]]*"scratches"' "$IMPORTED_MATERIAL_OUT_DIR/scene_runtime.json"
ld_cli_smoke_require_pattern '"kind"[[:space:]]*:[[:space:]]*"grime"' "$IMPORTED_MATERIAL_OUT_DIR/scene_runtime.json"
ld_cli_smoke_require_pattern '"mesh_asset_instances"[[:space:]]*:[[:space:]]*1' "$IMPORTED_MATERIAL_OUT_DIR/scene_summary.json"
ld_cli_smoke_require_pattern '"mesh_assets_copied"[[:space:]]*:[[:space:]]*1' "$IMPORTED_MATERIAL_OUT_DIR/scene_summary.json"
ld_cli_smoke_require_file "$IMPORTED_MATERIAL_OUT_DIR/assets/mesh_assets/asset_imported_tetrahedron_01.runtime.json"
ld_cli_smoke_require_file "$LINE_DIR/tmp/agent_imported_mesh_material_prompt/line_drawing_app_load/assets/mesh_assets/asset_imported_tetrahedron_01.runtime.json"
}

case "$MODE" in
  all)
    run_failure_cases
    run_success_cases
    ;;
  failures)
    run_failure_cases
    ;;
  success)
    run_success_cases
    ;;
  *)
    ld_cli_smoke_error "unknown mode '$MODE' (expected all, failures, or success)"
    exit 2
    ;;
esac

echo "[line-agent-scene] PASS ($MODE)"
