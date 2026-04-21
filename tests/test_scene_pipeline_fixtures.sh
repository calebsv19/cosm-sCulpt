#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
LINE_DIR="$ROOT/line_drawing"
PIPELINE="$LINE_DIR/tools/scene_export_compile_pipeline.sh"

require_pattern() {
  local pattern="$1"
  local path="$2"
  if ! rg -q "$pattern" "$path"; then
    echo "[line-scene-fixtures] ERROR: expected pattern '$pattern' in $path" >&2
    exit 1
  fi
}

reject_pattern() {
  local pattern="$1"
  local path="$2"
  if rg -q "$pattern" "$path"; then
    echo "[line-scene-fixtures] ERROR: unexpected pattern '$pattern' in $path" >&2
    exit 1
  fi
}

run_case() {
  local fixture="$1"
  local scene_id="$2"
  local out_dir="$3"
  local mode="$4"

  local authoring="$LINE_DIR/tmp/$out_dir/scene_authoring.json"
  local runtime="$LINE_DIR/tmp/$out_dir/scene_runtime.json"

  bash "$PIPELINE" \
    --layout "$LINE_DIR/tests/fixtures/$fixture" \
    --scene-id "$scene_id" \
    --authoring-out "$authoring" \
    --runtime-out "$runtime" \
    --determinism-check

  case "$mode" in
    baseline_2d)
      require_pattern '"object_type"[[:space:]]*:[[:space:]]*"curve_path"' "$runtime"
      reject_pattern '"primitive"[[:space:]]*:' "$runtime"
      ;;
    primitives_3d)
      require_pattern '"space_mode_default"[[:space:]]*:[[:space:]]*"3d"' "$authoring"
      require_pattern '"object_type"[[:space:]]*:[[:space:]]*"plane_primitive"' "$runtime"
      require_pattern '"object_type"[[:space:]]*:[[:space:]]*"rect_prism_primitive"' "$runtime"
      require_pattern '"primitive"[[:space:]]*:[[:space:]]*\{' "$runtime"
      require_pattern '"active_object3d_count"[[:space:]]*:[[:space:]]*2' "$authoring"
      ;;
    mixed_2d_3d)
      require_pattern '"space_mode_default"[[:space:]]*:[[:space:]]*"3d"' "$authoring"
      require_pattern '"object_type"[[:space:]]*:[[:space:]]*"curve_path"' "$runtime"
      require_pattern '"object_type"[[:space:]]*:[[:space:]]*"rect_prism_primitive"' "$runtime"
      require_pattern '"primitive"[[:space:]]*:[[:space:]]*\{' "$runtime"
      require_pattern '"active_object3d_count"[[:space:]]*:[[:space:]]*1' "$authoring"
      ;;
    *)
      echo "[line-scene-fixtures] ERROR: unknown mode $mode" >&2
      exit 1
      ;;
  esac
}

run_case "ld3d2_layout_fixture.json" "scene_line_drawing_ld3d2_smoke" "ld3d2_smoke" "baseline_2d"
run_case "ld3d3_primitive_layout_fixture.json" "scene_line_drawing_ld3d3_smoke" "ld3d3_smoke" "primitives_3d"
run_case "ld3d4_mixed_layout_fixture.json" "scene_line_drawing_ld3d4_smoke" "ld3d4_smoke" "mixed_2d_3d"

echo "[line-scene-fixtures] PASS"
