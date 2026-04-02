#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
LINE_DIR="$ROOT/line_drawing"
SCENE_COMPILE_BIN="$ROOT/shared/core/core_scene_compile/build/scene_compile_tool"

LAYOUT_PATH="$LINE_DIR/tests/fixtures/ld3d2_layout_fixture.json"
SCENE_ID="scene_line_drawing_ld3d2"
AUTHORING_OUT="$LINE_DIR/tmp/ld3d2/scene_authoring.json"
RUNTIME_OUT="$LINE_DIR/tmp/ld3d2/scene_runtime.json"
DETERMINISM_CHECK=0

SCENE_MATERIAL_ID=""
SCENE_MATERIAL_TYPE=""
SCENE_LIGHT_ID=""
SCENE_LIGHT_TYPE=""
SCENE_CAMERA_ID=""
SCENE_CAMERA_TYPE=""

usage() {
  cat <<USAGE
Usage: $(basename "$0") [options]

Run line_drawing authoring export -> shared compile pipeline.

Options:
  --layout <path>               Layout JSON input file.
  --scene-id <id>               Scene ID for export.
  --authoring-out <path>        Output path for authoring scene JSON.
  --runtime-out <path>          Output path for runtime scene JSON.
  --determinism-check           Compile twice and verify identical runtime output.
  --scene-material-id <id>      Optional scene material id override.
  --scene-material-type <type>  Optional scene material type override.
  --scene-light-id <id>         Optional scene light id override.
  --scene-light-type <type>     Optional scene light type override.
  --scene-camera-id <id>        Optional scene camera id override.
  --scene-camera-type <type>    Optional scene camera type override.
  -h, --help                    Show help.
USAGE
}

log() {
  printf '[line-scene-pipeline] %s\n' "$*"
}

require_file() {
  local path="$1"
  if [[ ! -f "$path" ]]; then
    echo "[line-scene-pipeline] ERROR: missing file: $path" >&2
    exit 1
  fi
}

parse_args() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --layout)
        shift
        LAYOUT_PATH="${1:-}"
        ;;
      --scene-id)
        shift
        SCENE_ID="${1:-}"
        ;;
      --authoring-out)
        shift
        AUTHORING_OUT="${1:-}"
        ;;
      --runtime-out)
        shift
        RUNTIME_OUT="${1:-}"
        ;;
      --determinism-check)
        DETERMINISM_CHECK=1
        ;;
      --scene-material-id)
        shift
        SCENE_MATERIAL_ID="${1:-}"
        ;;
      --scene-material-type)
        shift
        SCENE_MATERIAL_TYPE="${1:-}"
        ;;
      --scene-light-id)
        shift
        SCENE_LIGHT_ID="${1:-}"
        ;;
      --scene-light-type)
        shift
        SCENE_LIGHT_TYPE="${1:-}"
        ;;
      --scene-camera-id)
        shift
        SCENE_CAMERA_ID="${1:-}"
        ;;
      --scene-camera-type)
        shift
        SCENE_CAMERA_TYPE="${1:-}"
        ;;
      -h|--help)
        usage
        exit 0
        ;;
      *)
        echo "[line-scene-pipeline] ERROR: unknown option: $1" >&2
        usage
        exit 2
        ;;
    esac
    shift
  done
}

append_optional_arg() {
  local key="$1"
  local value="$2"
  if [[ -n "$value" ]]; then
    SHAPE_TOOL_ARGS+=("$key" "$value")
  fi
}

validate_outputs() {
  local authoring_path="$1"
  local runtime_path="$2"
  if ! rg -q '"schema_variant"[[:space:]]*:[[:space:]]*"scene_authoring_v1"' "$authoring_path"; then
    echo "[line-scene-pipeline] ERROR: authoring output missing scene_authoring_v1 schema_variant" >&2
    exit 1
  fi
  if ! rg -q '"schema_variant"[[:space:]]*:[[:space:]]*"scene_runtime_v1"' "$runtime_path"; then
    echo "[line-scene-pipeline] ERROR: runtime output missing scene_runtime_v1 schema_variant" >&2
    exit 1
  fi
}

main() {
  parse_args "$@"
  require_file "$LAYOUT_PATH"

  mkdir -p "$(dirname "$AUTHORING_OUT")" "$(dirname "$RUNTIME_OUT")" "$LINE_DIR/export"

  log "building line_drawing shape_tool"
  make -C "$LINE_DIR" shape_tool >/dev/null

  log "building shared scene compile tool"
  make -C "$ROOT/shared/core/core_scene_compile" scene-compile-tool >/dev/null

  local export_name="ld3d2_authoring_export_$$.json"
  local export_path="$LINE_DIR/export/$export_name"
  local runtime_tmp2="${RUNTIME_OUT%.json}.determinism.second.json"

  SHAPE_TOOL_ARGS=(
    "$LAYOUT_PATH"
    "--export-scene" "$export_name"
    "--scene-id" "$SCENE_ID"
  )
  append_optional_arg "--scene-material-id" "$SCENE_MATERIAL_ID"
  append_optional_arg "--scene-material-type" "$SCENE_MATERIAL_TYPE"
  append_optional_arg "--scene-light-id" "$SCENE_LIGHT_ID"
  append_optional_arg "--scene-light-type" "$SCENE_LIGHT_TYPE"
  append_optional_arg "--scene-camera-id" "$SCENE_CAMERA_ID"
  append_optional_arg "--scene-camera-type" "$SCENE_CAMERA_TYPE"

  log "exporting authoring scene from layout"
  (
    cd "$LINE_DIR"
    build/bin/shape_tool "${SHAPE_TOOL_ARGS[@]}" >/dev/null
  )
  require_file "$export_path"

  cp "$export_path" "$AUTHORING_OUT"

  log "compiling authoring -> runtime"
  "$SCENE_COMPILE_BIN" "$AUTHORING_OUT" "$RUNTIME_OUT" >/dev/null
  validate_outputs "$AUTHORING_OUT" "$RUNTIME_OUT"

  if [[ "$DETERMINISM_CHECK" -eq 1 ]]; then
    log "running deterministic compile check"
    "$SCENE_COMPILE_BIN" "$AUTHORING_OUT" "$runtime_tmp2" >/dev/null
    if ! cmp -s "$RUNTIME_OUT" "$runtime_tmp2"; then
      echo "[line-scene-pipeline] ERROR: deterministic compile check failed" >&2
      exit 1
    fi
    rm -f "$runtime_tmp2"
  fi

  rm -f "$export_path"

  log "PASS"
  log "authoring: $AUTHORING_OUT"
  log "runtime:   $RUNTIME_OUT"
}

main "$@"
