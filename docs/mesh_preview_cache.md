# Main Edit mesh preview working set

Main Edit prepares imported runtime meshes on one existing `core_workers 1.0.1`
worker, with eight outstanding jobs including results awaiting publication.
A camera render requests work and immediately draws native bounds proxies. The
UI thread polls immutable completed results; generation and source-stamp checks
reject canceled, replaced or removed assets. Scene reset cancels without waiting;
shutdown drains and joins the worker. Cancellation is cooperative between shared
decoder/LOD stages, so an active shared call can finish before cancellation takes
full effect. Original source geometry, physical bounds, measurement and export
remain authoritative. This changes preview preparation; the native layout JSON
load and authoring STL import compilation are still synchronous.

## Cache and input limits

The app owns a 1024-bucket path lookup and dynamic LRU cache with a 256 MiB
resident budget including metadata and indexed geometry. Hidden references remain
resident when possible. Warm camera requests perform no decode, hash or LOD work.
Referenced resolved paths are checked for size/inode/mtime changes at most every
500 ms during render activity; reference changes reconcile independently. This
checks known entries, rather than watching the filesystem while idle. A missing
uncached path still goes through the native resolver when requested again.

Each decoder input is a private immutable snapshot capped at 8 MiB; snapshot
copying checks cancellation and refuses growth beyond this limit. Jobs admit at
most 8 MiB of completed LOD geometry each (64 MiB total outstanding reservation).
A malformed, oversized, failed, unsimplifiable or uncacheable mesh remains a bounds
proxy. Resident cache, pending reservation, raster arrays, texture-size estimate,
planner scratch and process peak RSS are reported separately. Decoded cJSON and
shared simplifier allocations, driver allocations and upload staging are outside
the resident budget; this is not a proven total-process/GPU allocation ceiling.

## Prepared geometry

Four indexed preview levels (256 / 2,000 / 8,000 / 18,000 triangles) persist in an
app-owned `core_pack` store under `.cache/mesh_preview` in the runtime directory.
`LINE_DRAWING_PREVIEW_STORE` can select an isolated store. SHA256 source content
and preprocessing version bind entries. The v2 recipe invalidates previous v1
8k/18k entries without modifying their bytes. Metadata/payload hashes, byte order,
scalar representation, finite vertices, bounded chunk sizes and valid indices
reject corruption. A completed temporary file is atomically renamed. Prepared
files are capped at 8 MiB and the disk store at 256 MiB. Full stores refuse new
writes; reconstruction remains available. This disposable private representation
is not a portable shared preview schema. Recipe/representation changes require a
new preprocessing version.

## Visible detail

Conservative transformed object AABBs and existing assembly hierarchy bounds
reject offscreen objects before triangle traversal and mesh preparation. Unknown
bounds remain visible. Screen diameter chooses a preview level with 20 percent
hysteresis. Moving previews cap ordinary meshes at 8k; settled previews can use
18k. A 120,000-triangle whole-scene plan reduces smaller ordinary objects first,
then uses 12-triangle bounds proxies if needed. Selected objects and inspection
views retain the full preview level; unavoidable overflow is reported. Native
source/export geometry is never reduced. Planner capacity is 16,384 objects and
128 assemblies; larger scenes fall back to the established renderer. No spatial
index or persistent GPU 3D mesh/instance rendering is claimed.

Moving raster scale is 0.50 and settled scale is 0.75. The diagnostics report
resident assets, pending preparation, frame time, proxies and detail-limit overflow.

## Native qualification

`LINE_DRAWING_NAVIGATION_PROOF_LAYOUT` and `LINE_DRAWING_NAVIGATION_PROOF_OUTPUT`
select a read-only scene and fresh existing output directory. The route measures
layout load, first proxy frame, loading movement frames, time until preparation
finishes and 90 warm pan/orbit frames. It checks unchanged native document state,
no warm decode/LOD rebuilding, zero UI-thread preview decodes, settled quality and
guide eligibility. Captures cover movement, settled and editor guide states.
Proof-only width/height overrides accept 800–1920 by 600–1080 logical pixels.
`LINE_DRAWING_NAVIGATION_PROOF_SWITCHES` exercises up to 16 repeated scene loads.
`LINE_DRAWING_NAVIGATION_PROOF_FOCUS_FIRST` frames the first object for offscreen
proofs. Detail/culling oracle switches are diagnostic comparisons only.

2026-10-09 exact development package qualification measures roof/exterior/interior
warm p95 of 30.899 / 26.324 / 21.239 ms at 1280x720 logical pixels (2560x1440
Retina captures). Roof empty-store first proxy frame is 47.588 ms after a 93.585 ms
native layout load; all preparation completes in 545.379 ms including that first
frame. Loading navigation worst is 29.432 ms. Reopening reuses 93 prepared assets
with zero runtime decode/LOD builds. These short automated runs establish those
inputs; live human navigation acceptance remains separate.

## Editor hitbox bounds

Editor hitbox rebuilding uses native mesh-instance bounds and the complete
rotation/positive nonuniform scale transform. It does not load runtime preview
statistics per instance. Selection remains available when a preview file is
unavailable; mesh loading is a separate preview concern. This removes repeated
source parsing from large repeated-instance editor updates.

## Pass 1 camera signature refinement (2026-10-11)

Object visibility uses already-resolved metadata during render traversal. The
filled-surface signature hashes effective eligibility, geometry/transforms, actual
preview color, reference status and parent/frame identity; inactive engineering
property slots do not enter that key. Camera projection and appearance/style remain
separate signatures. Per-frame signature construction no longer resolves mesh
paths. The preview cache still checks known source stamps during render activity
and invalidates pixels when assets change or prepared results arrive.

Rapid replacement/reset/shutdown tests cover repeated canceled generations,
source replacement, empty-scene reconciliation and restart. These checks establish
those cases, not the absence of every possible concurrency bug.

## Preview shutdown qualification — 2026-10-15

See Main Edit `docs/mesh_preview_shutdown.md`: bounded app-local input readers
retain existing shared workers and source validation. Four focused cases and
637 cases / 70 groups pass. Three native restored-workspace quit checks preserve
scene/history/draft state and finish shutdown in 14–16 ms. The previous acceptance
blocker is resolved in the private package; adoption/Desktop refresh remain next.
