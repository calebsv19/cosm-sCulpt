#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
LINE_DIR="$ROOT/line_drawing"
OUT_DIR="$LINE_DIR/tmp/agent_room_prisms"
REQUEST="$LINE_DIR/tests/fixtures/agent_room_prisms_request.json"
GALLERY_OUT_DIR="$LINE_DIR/tmp/agent_gallery_room_blocks_v2/line_drawing"
GALLERY_REQUEST="$LINE_DIR/tests/fixtures/agent_gallery_room_blocks_v2_request.json"
EMITTER_OUT_DIR="$LINE_DIR/tmp/agent_floor_prism_emitter/line_drawing"
EMITTER_REQUEST="$LINE_DIR/tests/fixtures/agent_floor_prism_emitter_request.json"
TOOL_REL="$(make -s -C "$LINE_DIR" print-agent-scene-tool-bin)"
TOOL="$LINE_DIR/$TOOL_REL"

if [[ -z "$TOOL_REL" || ! -x "$TOOL" ]]; then
  echo "[line-agent-scene] ERROR: agent_scene_tool binary not found at $TOOL" >&2
  exit 1
fi

require_pattern() {
  local pattern="$1"
  local path="$2"
  if ! rg -q "$pattern" "$path"; then
    echo "[line-agent-scene] ERROR: expected pattern '$pattern' in $path" >&2
    exit 1
  fi
}

rm -rf "$OUT_DIR"
"$TOOL" --request "$REQUEST" --out "$OUT_DIR" --determinism-check >/dev/null

require_pattern '"schema"[[:space:]]*:[[:space:]]*"line_drawing_agent_scene_request_v1"' "$OUT_DIR/agent_request.json"
require_pattern '"objectType"[[:space:]]*:[[:space:]]*"plane_primitive"' "$OUT_DIR/layout.json"
require_pattern '"objectType"[[:space:]]*:[[:space:]]*"rect_prism_primitive"' "$OUT_DIR/layout.json"
require_pattern '"schema_variant"[[:space:]]*:[[:space:]]*"scene_authoring_v1"' "$OUT_DIR/scene_authoring.json"
require_pattern '"schema_variant"[[:space:]]*:[[:space:]]*"scene_runtime_v1"' "$OUT_DIR/scene_runtime.json"
require_pattern '"object_type"[[:space:]]*:[[:space:]]*"plane_primitive"' "$OUT_DIR/scene_runtime.json"
require_pattern '"object_type"[[:space:]]*:[[:space:]]*"rect_prism_primitive"' "$OUT_DIR/scene_runtime.json"
require_pattern '"app_layout"[[:space:]]*:[[:space:]]*"' "$OUT_DIR/scene_summary.json"
require_pattern '"app_authoring"[[:space:]]*:[[:space:]]*"' "$OUT_DIR/scene_summary.json"
require_pattern '"app_runtime"[[:space:]]*:[[:space:]]*"' "$OUT_DIR/scene_summary.json"
require_pattern '"planes"[[:space:]]*:[[:space:]]*3' "$OUT_DIR/scene_summary.json"
require_pattern '"rect_prisms"[[:space:]]*:[[:space:]]*3' "$OUT_DIR/scene_summary.json"
test -f "$OUT_DIR/scene_agent_room_prisms.layout.json"
test -f "$OUT_DIR/line_drawing_app_load/scene_authoring.json"
test -f "$OUT_DIR/line_drawing_app_load/scene_runtime.json"

rm -rf "$LINE_DIR/tmp/agent_gallery_room_blocks_v2"
"$TOOL" --request "$GALLERY_REQUEST" --out "$GALLERY_OUT_DIR" --determinism-check >/dev/null

require_pattern '"planes"[[:space:]]*:[[:space:]]*1' "$GALLERY_OUT_DIR/scene_summary.json"
require_pattern '"rect_prisms"[[:space:]]*:[[:space:]]*6' "$GALLERY_OUT_DIR/scene_summary.json"
require_pattern '"bounds_adjusted"[[:space:]]*:[[:space:]]*0' "$GALLERY_OUT_DIR/scene_summary.json"
test -f "$LINE_DIR/tmp/agent_gallery_room_blocks_v2/agent_gallery_room_blocks_v2.layout.json"
test -f "$LINE_DIR/tmp/agent_gallery_room_blocks_v2/line_drawing_app_load/scene_authoring.json"
test -f "$LINE_DIR/tmp/agent_gallery_room_blocks_v2/line_drawing_app_load/scene_runtime.json"

rm -rf "$LINE_DIR/tmp/agent_floor_prism_emitter"
"$TOOL" --request "$EMITTER_REQUEST" --out "$EMITTER_OUT_DIR" --determinism-check >/dev/null

require_pattern '"object_id"[[:space:]]*:[[:space:]]*"floor"' "$EMITTER_OUT_DIR/scene_runtime.json"
require_pattern '"object_id"[[:space:]]*:[[:space:]]*"emitter_prism"' "$EMITTER_OUT_DIR/scene_runtime.json"
require_pattern '"physics_sim"[[:space:]]*:' "$EMITTER_OUT_DIR/scene_runtime.json"
require_pattern '"scene_domain"[[:space:]]*:' "$EMITTER_OUT_DIR/scene_runtime.json"
require_pattern '"object_overlays"[[:space:]]*:' "$EMITTER_OUT_DIR/scene_runtime.json"
require_pattern '"motion_mode"[[:space:]]*:[[:space:]]*"Static"' "$EMITTER_OUT_DIR/scene_runtime.json"
require_pattern '"motion_mode"[[:space:]]*:[[:space:]]*"Dynamic"' "$EMITTER_OUT_DIR/scene_runtime.json"
require_pattern '"emitter"[[:space:]]*:' "$EMITTER_OUT_DIR/scene_runtime.json"
require_pattern '"strength"[[:space:]]*:[[:space:]]*80' "$EMITTER_OUT_DIR/scene_runtime.json"
test -f "$LINE_DIR/tmp/agent_floor_prism_emitter/agent_floor_prism_emitter.layout.json"
test -f "$LINE_DIR/tmp/agent_floor_prism_emitter/line_drawing_app_load/scene_authoring.json"
test -f "$LINE_DIR/tmp/agent_floor_prism_emitter/line_drawing_app_load/scene_runtime.json"

echo "[line-agent-scene] PASS"
