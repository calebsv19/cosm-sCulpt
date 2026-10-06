#!/bin/sh
# Package diagnostics must not seed or replace the user's runtime/log lanes.
set -eu
launcher="$1"
mode="${2:---self-test}"
case "$mode" in --self-test|--print-config) ;; *) exit 64 ;; esac
test -x "$launcher"
root="$(mktemp -d "${TMPDIR:-/tmp}/sculpt-package-self-test.XXXXXX")"
trap 'rm -rf "$root"' EXIT HUP INT TERM
run_launcher() {
    env -u VK_ICD_FILENAMES -u VK_DRIVER_FILES -u VK_RENDERER_SHADER_ROOT \
        -u SHAPE_ASSET_DIR \
        LINE_DRAWING_RUNTIME_DIR="$root/runtime" \
        LINE_DRAWING_LOG_DIR="$root/logs" "$launcher" "$1"
}
if run_launcher "$mode"; then
    exit 0
else
    status=$?
    echo "Package diagnostic failed; isolated launcher config follows:" >&2
    run_launcher --print-config >&2 || true
    exit "$status"
fi
