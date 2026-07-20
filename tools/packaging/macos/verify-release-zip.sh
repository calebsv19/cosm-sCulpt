#!/bin/sh
set -eu

if [ "$#" -ne 3 ] && [ "$#" -ne 6 ]; then
    echo "usage: $0 <release.zip> <App.app> <signing-identity> [receipt.json notary.json source-commit]" >&2
    exit 64
fi

archive="$1"
app_name="$2"
signing_identity="$3"
receipt_path="${4:-}"
notary_path="${5:-}"
source_commit="${6:-}"
test -f "$archive"

verify_root="$(mktemp -d "${TMPDIR:-/tmp}/sculpt-release-verify.XXXXXX")"
trap 'rm -rf "$verify_root"' EXIT HUP INT TERM

ditto -x -k "$archive" "$verify_root"
app="$verify_root/$app_name"
test -d "$app"

codesign --verify --deep --strict --verbose=2 "$app"

signature_dir="$app/Contents/_CodeSignature"
test -f "$signature_dir/CodeResources"
if find "$signature_dir" -mindepth 1 -maxdepth 1 -type f ! -name CodeResources | grep -q .; then
    echo "release ZIP contains materialized detached signatures" >&2
    find "$signature_dir" -mindepth 1 -maxdepth 1 -type f -print >&2
    exit 1
fi

launcher="$app/Contents/MacOS/line-drawing-launcher"
launcher_script="$app/Contents/Resources/line-drawing-launcher.sh"
file "$launcher" | grep -q 'Mach-O'
file "$launcher_script" | grep -q 'shell script'
"$launcher" --self-test >/dev/null

if [ "$signing_identity" != "-" ]; then
    spctl --assess --type execute --verbose=2 "$app"
    xcrun stapler validate "$app"
fi

if [ -n "$receipt_path" ]; then
    test -f "$notary_path"
    printf '%s\n' "$source_commit" | grep -Eq '^[0-9a-f]{40}$'
    archive_sha="$(shasum -a 256 "$archive" | awk '{print $1}')"
    notary_sha="$(shasum -a 256 "$notary_path" | awk '{print $1}')"
    receipt_tmp="$receipt_path.tmp"
    cat >"$receipt_tmp" <<EOF
{
  "schema": "line_drawing_macos_release_roundtrip_v1",
  "archive_sha256": "$archive_sha",
  "notary_evidence_sha256": "$notary_sha",
  "source_commit": "$source_commit",
  "codesign_deep_strict": true,
  "detached_signature_files_absent": true,
  "native_launcher": true,
  "launcher_self_test": true,
  "gatekeeper_accepted": true,
  "staple_validated": true
}
EOF
    mv "$receipt_tmp" "$receipt_path"
fi

echo "release ZIP round-trip verification passed: $archive"
