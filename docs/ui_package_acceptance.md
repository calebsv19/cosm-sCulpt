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

# UI package acceptance checkpoint

## 2026-10-15 — UI package verified; real-workspace exit blocker remains

An isolated Main Edit package was built from the exact HUD-qualified source
checkpoint. Package self-test, local-development identity, icon/namespaces,
private installation readback, bundled dependencies and strict ad-hoc signature
verification pass. Twelve packaged runtime modes pass through the bundled launcher
and MoltenVK, including both eight-stage window paths and validated startup/resize/
restart with zero warnings/errors. Seven retained frames exactly match the prior
source checkpoint. The prior full source gate remains 633 cases / 69 groups;
no functional source changed in this packaging slice.

Desktop Capture and native input reviewed the live launcher, restored 1297-object
van workspace, Font/Theme overlay, keyboard cancellation and real Unicode clipboard
entry into the Layouts filter. No scene edits or Save were performed. On quitting
that restored workspace, shutdown blocked joining a mesh-preview worker inside
fopen/open. The sample points to Layout_MeshPrepareShutdown -> core_workers shutdown
and the prepare worker opening input. Isolated fixture runs exit normally; this
real-workspace path is an unresolved acceptance blocker, not a qualified shutdown.
The task-owned stuck candidate was terminated after preserving the stack trace.
Do not mark the UI rollout fully complete or refresh Desktop until this is repaired
and the affected workspace's exit is reverified.

The native app-control route launched the candidate using the existing Main Edit
runtime despite the separate private launch. Its generated Vulkan ICD pointer was
restored byte-for-byte. All saved data, configuration, Desktop bundles, 15 directory
links and 3347 retained build files match baseline. The existing launcher log gained
841 bytes of review diagnostics; its entire original prefix hash is verified and
the append is retained as evidence. No log truncation or user-data rollback.

Evidence: `_private_workspace_artifacts/line_drawing/shared_ui_package_20261015/`.
The exact candidate bundle and compact logs/captures/sample are retained for the
exit-repair consumer. Disposable source/build/test homes are removed after consumer
checks. This is local development evidence, without commit, canonical adoption,
VERSION edit, Desktop refresh, publication or Registry promotion.
