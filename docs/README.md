# sCulpt Docs Index

Last updated: 2026-05-25

Start here for public repository documentation.

Public identity:
- packaged desktop product: `sCulpt`
- repository/program key: `line_drawing`

## Scaffold State
- `docs/current_truth.md`: current scaffold/runtime structure and verification snapshot.
- `docs/future_intent.md`: planned scaffold convergence and remaining migration slices.
- `docs/desktop_packaging.md`: `.app` packaging commands, launcher diagnostics, and Desktop refresh workflow.

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
- `make -C line_drawing test-legacy`
- `make -C line_drawing scene-pipeline-smoke`
- `make -C line_drawing package-desktop`
- `make -C line_drawing package-desktop-self-test`
- `make -C line_drawing package-desktop-refresh`
  - package output now rebuilds from explicit `PACKAGE_TOOLCHAIN` source
    binaries under `build/toolchains/<toolchain>/`

## Public Runtime Docs
- `README.md` (repo root): product/runtime overview and build/run flow.
- `docs/KEYBINDINGS.md`: current runtime keybind reference.
- `docs/agent_scene_authoring_cli.md`: headless agent-authored scene request and output directory contract.
- `src/*/README.md`: module-level current-state references for the rehomed source tree (`adapters/`, `panel/`, `overlay/`, `scene/`, `primitives/`, `mouse/`, `render/`, `gizmo/`).

## Current Published State
- `line_drawing` remains the canonical upstream authoring/export source for the current primitive scope.
- public product-facing docs should treat `sCulpt` as the primary app name and
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
- default launch now enters a top-level host menu before the editor session:
  - host menu owns high-level entry and reopen actions
  - editor remains the existing interactive authoring session behind that host seam
- the host menu is no longer one flat action list:
  - left navigation switches between quick actions, recents, layouts, scenes, and browse
  - center content lists recent contexts plus entries from the current input root
  - right detail space now shows selected-item context, cached metadata, and lightweight previews
  - `Esc` returns from the editor to the host menu instead of directly quitting the app
- the current worktree also exposes a concrete scene-directory export path:
  - `Export Scene` writes a named directory under the configured output root
  - each scene directory contains `scene_authoring.json` and compiled `scene_runtime.json`
  - runtime compilation flows through shared `core_scene_compile`
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
