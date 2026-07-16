# Tests

The main test harness is a lightweight C executable that links against the app
code minus the executable entry point. Each C test file exposes a
`*_run_tests` function, which the shared runner calls and reports on. Shell and
Python tests cover tool and agent-scene routes that are better exercised
outside the host C runner.

## Files
- `test_framework.h` / `.c` — minimal assertion macros plus `run_test_cases`, which prints per-group results.
- `test_artifact_helpers.h` — shared test-only helpers for repeated fixture
  artifacts: directory creation, text-file writes, canonical
  `scene_authoring.json` / `scene_runtime.json` path construction, authored
  scene-contract fixture writes, unique temp-directory setup, and runtime-state
  cleanup for File-pane/browser/recents fixtures.
- `test_math.c` — exercises `Math/math_util.h` helpers (snapping and coordinate conversions).
- `test_layout.c`, `test_layout_core.c`, `test_layout_object3d*.c`,
  `test_layout_hitbox.c`, and `test_layout_scene_export.c` — cover layout
  wall/anchor management, 3D object storage/resizing, hitbox ranking,
  JSON/schema round-trips, scene export metadata, and undo/redo behavior.
- `test_scene_export.c`, `test_shape_dataset.c`, and
  `test_scene_pipeline_fixtures.sh` — cover scene export, diagnostics dataset
  output, and scene-pipeline fixtures.
- `test_ui_panel_*.c` — cover scene list/menu behavior, view/create/object
  summaries, File summary/browser state, async STL failure visibility, and
  File-pane action-status parity for common load/import/export failures.
- `test_ui_panel_file_browser_session.c` — covers persistent File-browser
  session/root semantics, active/remembered row status, and restore-summary
  diagnostics as a focused group separate from heavier File-pane mesh/STL
  browser coverage.
- `test_host_menu.c`, `test_scene_catalog.c`, `test_catalog_preview.c`,
  `test_recent_contexts.c`, and `test_root_browser.c` — cover the menu-first
  host, recents, catalog scan/preview, and root suggestion surfaces.
- `test_startup_config.c` — covers startup data-root fallback diagnostics for
  unset, missing, and unchanged root paths without scraping startup stderr.
- `test_object_authoring.c` and `test_object_face_sketch.c` — cover object/CAD
  authoring documents, stable ids, sketch/extrude flows, replay/evaluation,
  persistence, and runtime mesh compile affordances.
- `test_render_readonly_contract.c` — static contract coverage for pure
  render/summary source files, ensuring they avoid known mutating app-state
  entry points. Mixed interaction/render surfaces are documented separately
  instead of forced into this contract.
- `test_input_policy.c`, `test_pane_host.c`, and
  `test_workspace_authoring_host.c` — cover input-policy precedence, pane-host
  solving, and workspace-authoring host behavior.
- `test_app_wrapper_diagnostics.c` — covers the app-wrapper stage diagnostic
  contract and asserts that config-adjacent wrapper stages do not claim
  runtime data-root, recent-context, File-browser restore, or package runtime
  setup ownership.
- `test_shutdown_lifetime.c` — covers `Global_Shutdown(...)` no-op/idempotent
  behavior and proves shutdown waits for and clears the File-pane async STL
  worker handle before releasing global state.
- `test_viewport3d_bridge.c` — proves the exact `FreeViewCamera` + Grid
  effective-target conversion, round-trip projection, shared pan/anchor zoom,
  orbit storage, resize effective-target preservation, and invalid-input
  nonmutation for the `core_viewport3d >= 0.1.0` adapter.
- `test_viewport_navigation_input.c` and `test_viewport_navigation_parity.c`
  — preserve LineDrawing input/arbitration and EVN rollback-oracle coverage
  around the shared bridge cutover.
- `test_imported_mesh_harness.sh` — covers deterministic imported mesh harness
  fixtures plus missing/invalid input diagnostics.
- `test_agent_scene_tool.sh` and `test_agent_scene_refine_lighting_policy.py`
  — cover agent scene request/export/refinement and RayTracing-facing lighting
  policy expansion, including missing/bad request diagnostics for the CLI.
- `lib/cli_smoke_helpers.sh` — shared shell helpers for CLI smoke scripts:
  expected-failure checks, regex assertions, file assertions, and temp failure
  directory reset.
- `test_runner.c` — entry point invoked by `make test`; aggregates all suites and emits a final pass/fail result.

## Running

```sh
make test
make test ARGS=StartupConfig
make test ARGS=UIPanelFileBrowser
make test ARGS=UIPanelFileBrowserSession
make test ARGS=Viewport3DBridge
make test ARGS=ViewportNavigationInput
make test ARGS=ViewportNavigationParity
make agent-scene-failure-smoke
make agent-scene-smoke
```

This command compiles the application sources into `build/toolchains/clang/obj`, builds the host test objects under `build/host/tests/obj`, links them into `build/host/tests/bin/run_tests`, and runs the binary. Use `DEBUG=1` with the command to compile the entire stack with debug flags.
Pass a test group name through `ARGS` to run one focused C suite without
changing the default full-suite invocation.
Run File-browser groups sequentially when using focused invocations; those
tests intentionally clear shared `data/runtime/*` fixture state.
Use `agent-scene-failure-smoke` for fast agent-scene CLI failure diagnostics;
`agent-scene-smoke` remains the broad success+failure smoke lane.
