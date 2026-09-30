# Sculpt Docs Index

Last updated: 2026-09-29

Start here for public repository documentation.

- [Links: attachment, support and containment](relationships.md) — S3a controls, schema and proof.
- [Parts: semantic organization and assemblies](semantic_assemblies.md) — S2 controls, schema, proof and limits.

Public identity:
- packaged desktop product: `Sculpt`
- repository/program key: `line_drawing`

## Spatial Engineering Direction

- [S1 engineering audit](s1_engineering_audit.md): UI workflows, bounded movement,
  verification levels, remaining CAD limits and the next S2 slice.

- [Persistent constraints](geometric_constraints.md): maintained distance rules,
  coincident points, planar mates, linear travel, hinges and atomic mutation coverage.

- [Geometric measurements](geometric_measurements.md): selectable primitive
  origins, axes and faces; distance/angle measurements, exact one-time gap/point
  placement, and links to persistent rule controls.

- [Engineering document contract](engineering_document_contract.md): implemented
  S0/S1a physical context, identity persistence and numerical edits with proof limits.

- [Spatial engineering direction](spatial_engineering_direction.md): proposed
  product goals, inspected foundations, and iterative delivery toward accurate
  physical layout, semantic scenes and agent editing. This is future direction,
  not a claim that the engineering feature set is implemented.

## Scaffold State
- `docs/current_truth.md`: current scaffold/runtime structure and verification snapshot.
- `docs/future_intent.md`: planned scaffold convergence and remaining migration slices.
- `docs/desktop_packaging.md`: `.app` packaging commands, launcher diagnostics, and Desktop refresh workflow.
- `docs/main_edit_worktree.md`: persistent Main Edit topology, isolated local
  app identity, lifecycle gates, and specialist-worktree preservation rules.
- `docs/memory_check_audit.md`: default-off fisiCs memory-check audit lane and
  the current clean report for the instrumented test runner.

Current verification contract:
- `make -C line_drawing clean && make -C line_drawing`
- `make -C line_drawing toolchain-contract`
- `make -C line_drawing dump-sema-canonical-scene-export`
- `make -C line_drawing dump-sema-canonical-scene-export-primitives`
- `make -C line_drawing dump-sema-scene-import`
- `make -C line_drawing clang-build`
- `make -C line_drawing fisics-build`
- `make -C line_drawing test-stable`
- `make -C line_drawing run-headless-smoke`
  - currently routes through `test-stable` rather than a separate runtime-only lane
- `make -C line_drawing visual-harness`
  - build-only readiness gate, not an unattended execution surface
- `make -C line_drawing visual-artifact`
  - source-run first-frame image proof; writes
    `line_drawing/visual_artifacts/line_drawing_first_frame.bmp`
  - expected success line:
    `visual-artifact: <absolute artifact path>`
- `make -C line_drawing visual-artifact-editor`
  - source-run editor/viewport first-frame image proof; writes
    `line_drawing/visual_artifacts/line_drawing_editor_first_frame.bmp`
  - expected success line:
    `visual-artifact-editor: <absolute artifact path>`
  - requires access to the local display session because it creates the
    development SDL/Vulkan window long enough to render and capture one frame
- `make -C line_drawing test-legacy`
- `make -C line_drawing scene-pipeline-smoke`
- `make -C line_drawing memory-check-audit`
  - default-off fisiCs memory-check lane; currently reaches the final summary
    with `active=0`, `double_free=0`, `unknown_free=0`, and
    `tracker_failures=0`
- `make -C line_drawing package-desktop`
- `make -C line_drawing package-desktop-print-config`
- `make -C line_drawing package-desktop-self-test`
- `make -C line_drawing package-desktop-refresh`
  - package output now rebuilds from explicit `PACKAGE_TOOLCHAIN` source
    binaries under `build/toolchains/<toolchain>/`
- `make -C line_drawing package-desktop-main-edit-self-test`
  - verifies `sCulpt Main Edit.app`, its separate bundle/runtime/log identity,
    and the exact generic source/binary build identity

## Public Runtime Docs
- `README.md` (repo root): product/runtime overview and build/run flow.
- `docs/KEYBINDINGS.md`: current runtime keybind reference.
- `docs/agent_scene_authoring_cli.md`: headless agent-authored scene request and output directory contract.
- `src/README.md` and `src/*/README.md`: source-lane routing references for
  the app wrapper, Core, Menu, UI, Layout, Input, Editor, ObjectAuthoring,
  Render, Math, and Tools. Dense implementation subtrees with local ownership
  maps now include `src/UI/panel`, `src/UI/overlay`, `src/UI/topbar`,
  `src/Layout/asset`, `src/Layout/primitives`, `src/Layout/scene`,
  `src/Input/mouse`, `src/Editor/gizmo`, and `src/Editor/render`.

## Current Published State
- `line_drawing` remains the canonical upstream authoring/export source for the current primitive scope.
- public product-facing docs should treat `Sculpt` as the primary app name and
  use `line_drawing` where repo/runtime identifiers need to stay exact
- the editor pane system is now structurally unified into five durable tabs:
  - left:
    - `Scene`
    - `File`
  - right:
    - `View`
    - `Create`
    - `Object`
  - each pane now follows the same ownership model:
    - compact top summary
    - primary middle working surface
    - anchored lower actions/controls where needed
- the scene-editor usability pass now keeps creation and scene-record editing
  usable at constrained pane heights:
  - `Create` is context-switched by Geometry, Paths, Lighting, Materials, and
    Construction intent instead of showing every authoring action at once
  - variable-height right-pane content has wheel and draggable-scrollbar
    overflow rather than clipping or overlapping controls
  - scene-record labels and continuous camera/path/light properties use direct
    validated entry with one undo unit per accepted edit
  - path editing exposes explicit `Place Point` and `Remove Point` actions in
    addition to viewport handles and keyboard accelerators
- default launch now enters a top-level host menu before the editor session:
  - host menu owns high-level entry and reopen actions
  - editor remains the existing interactive authoring session behind that host seam
- the host menu is no longer one flat action list:
  - left navigation switches between quick actions, recents, layouts, scenes, and browse
  - center content lists recent contexts plus entries from the current input root
  - right detail space now shows selected-item context, cached metadata, and lightweight previews
  - `Esc` returns from the editor to the host menu instead of directly quitting the app
- the current worktree also exposes a concrete scene-directory export path:
  - `Export Runtime` writes one named package directory under the configured output root
  - each package contains `scene_package.json`, editable `scene_authoring.json`, compiled `scene_runtime.json`, dependencies, and a receipt
  - runtime compilation flows through shared `core_scene_compile`
  - active scene paths are naming hints only; the configured output root owns
    the export destination
  - export success/failure is visible in the File pane and then returns to the
    normal action label
- the editor topbar is now the production top-level status/action lane:
  - `Mode`, `View`, `Plane`, `Bounds`, `Gizmo`, `Undo`, and `Redo` chips mirror
    the existing editor actions
  - active center-gizmo drags report live move, rotate, or size operation
    values through the topbar
- the compiler-units rollout now starts in the authoring/export seam:
  - first sema customer: `src/Tools/canonical_scene_export.c`
  - second sema customer: `src/Tools/canonical_scene_export_primitives.c`
  - third sema customer: `src/Tools/scene_import.c`
  - first explicit root authoring metadata options:
    - `world_scale`
    - `unit_system`
    - `conversion_policy`
  - the first primitive/export seam now carries explicit scene-space lengths for:
    - primitive width / height / depth payloads
    - framing-bounds fallback half-extents
    - framing-bounds padding
  - the import seam now validates round-trip authoring metadata for:
    - `world_scale`
    - `unit_system`
    - `conversion_policy`
  - the app/package build contract is now split so Clang and `fisiCs` app
    outputs no longer share one package source binary path

## Private Planning Docs
- Private scaffold plans and internal execution docs are in the workspace private docs bucket:
  - `../../docs/private_program_docs/line_drawing/`
