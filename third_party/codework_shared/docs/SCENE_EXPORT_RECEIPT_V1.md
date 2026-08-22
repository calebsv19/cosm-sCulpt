# Scene Export Receipt v1

Status: stable shared publication contract

Schema family: `codework_scene_export`

Schema variant: `scene_export_receipt_v1`

Owner: `core_scene_compile`

## Boundary

`scene_authoring.json` is the editable, portable scene source exchanged between
authoring programs. `scene_runtime.json` is deterministic derived output and is
never an editable source. A LineDrawing layout JSON remains an app-private fast
load document; it is not a cross-program scene or a promoted runtime.

Compilation and publication are separate operations. Compilation binds source
and dependencies into deterministic runtime bytes. Publication adds a receipt
and makes all three files visible with one directory rename.

## Published bundle

```text
<scene-bundle>/
  scene_authoring.json
  scene_runtime.json
  scene_export_receipt.json
```

Publication is create-only. If `<scene-bundle>` already exists, the publisher
fails without modifying it. The caller must select a new iteration name. Files
are written and synchronized in a sibling staging directory; the staging
directory is then renamed to the final path. Failed staging work is removed.

## Receipt fields

The receipt records:

- compiler name, semantic version, and normalization version;
- SHA-256 and byte count for authoring and runtime artifacts;
- dependency-set SHA-256 and dependency count;
- bundle SHA-256;
- publication mode and publication time.

The bundle digest is SHA-256 over this canonical UTF-8 input:

```text
authoring:<authoring sha256>
runtime:<runtime sha256>
dependencies:<dependency-set sha256>
compiler:<compiler version>
normalization:<normalization version>
```

Publication time is receipt evidence and is excluded from the bundle digest.
The runtime digest cannot be embedded in runtime JSON because that would create
a self-referential digest.

## Dependency digest

The compiler accepts a caller-calculated SHA-256 for the canonical dependency
set plus its count. With no external dependencies, the digest is SHA-256 of the
empty byte sequence and the count is zero. Producers that reference external
assets must canonicalize and hash those assets before claiming a non-empty set;
path discovery and asset packaging remain producer responsibilities.

## Consumer rules

- Load `scene_authoring.json` for editing.
- Consume `scene_runtime.json` only when its receipt digests verify.
- Reject unknown receipt schema variants or unsupported compiler versions.
- Do not infer promotion, simulation completion, or render readiness from a
  successful export receipt. Those require separate typed receipts.
