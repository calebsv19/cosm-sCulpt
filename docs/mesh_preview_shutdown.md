# Mesh-preview input and shutdown contract

## 2026-10-15 — Mesh-preview shutdown blocker repaired and requalified

The preview worker could block in source-file open before it could observe
cancellation; shutdown then waited in the existing shared worker-pool join.
App-local `layout_mesh_source_snapshot.c` confines source open/read to one bounded
reader child at a time. The child uses only async-signal-safe POSIX operations;
the parent retains decoding and publication. Cancellation or the 2-second input
read deadline kills and reaps that child, removes its temporary snapshot and
leaves the preview unavailable. Source size, regular-file/stamp checks, content
revalidation, stale-result rejection and existing shared worker ownership remain.
No scene, editor/save/draft contract or shared API/version changed. The underlying
OS reason for the earlier blocked open is not established by the stack sample.

Build, four focused snapshot tests, existing ten VanUsability cases, full 637
cases in 70 groups, package self-test/contracts and export smoke pass. Tests cover
blocked-child deadline, cancellation/reaping, byte-exact snapshots, changed stamps,
pre-cancel and FIFO rejection. Three native packaged quit proofs each restore the
byte-identical 1297-object van workspace, render with eight previews pending,
retain document/history/saved state and an unsaved root-dialog draft, then send
SDL_QUIT through App_Run and call the ordinary runtime shutdown. Shutdown takes
14, 14 and 16 ms; each launcher exits zero, with no Vulkan validation diagnostics.
The real Vulkan frame was inspected. A supervised live test also exits zero on
an SDL shutdown signal; native accessibility could not reliably attach, so this
is native event-path qualification rather than a repeated manual window-close claim.

All nine protected roots, 15 runtime resource links and 3347 retained build files
match the start of this repair. The original scene hash is unchanged. The prior
841-byte launcher-log diagnostic append remains preserved; this repair adds none.
Source stays uncommitted in retained Main Edit; canonical and both Desktop apps
remain unchanged. No VERSION/release/publication/Registry action occurred.

Evidence: `_private_workspace_artifacts/line_drawing/mesh_preview_exit_20261015/`.
The earlier package acceptance blocker is resolved at the source/private-package
layer. Next: reviewed source checkpoint/adoption and session-safe Main Edit
Desktop refresh, with installed-byte readback. Broad native IME candidate/glyph
acceptance remains separate from the qualified routing and Unicode clipboard checks.

## Owned temporary snapshot paths

The source-reader boundary uses the invocation's existing absolute `TMPDIR`.
An absent, relative, unavailable or unwritable root makes the preview unavailable;
it does not fall back to a shared absolute temporary directory or create a root.
Snapshot names are allocated to the exact root-plus-template size and created
with `mkstemp` (0600, unique exclusive creation) and close-on-exec descriptors.
The internal API returns an owned path; the preparation job unlinks and frees
both its initial input snapshot and its later source-verification snapshot after
their last consumers finish, including cancellation and refusal exits. Existing
source-stamp validation, content revalidation, child deadlines/reaping and stale
result rejection remain unchanged. Long paths no longer depend on a128-byte
output buffer. The new tests qualify this filesystem boundary; they do not
resolve or waive intermittent native window/restart readiness failures.
