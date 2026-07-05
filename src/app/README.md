# App Wrapper

This lane owns the scaffold lifecycle wrapper for `line_drawing`.

## Files

- `line_drawing_app_main.c` — implements `line_drawing_app_main(...)` and the
  stage-ordered wrapper behind `include/line_drawing/line_drawing_app_main.h`.
  It owns bootstrap, config-load, state-seed, subsystem-init, runtime-start,
  run-loop handoff, dispatch accounting, stage-order diagnostics, and shutdown
  sequencing. It also exposes read-only stage diagnostics through
  `line_drawing_app_stage_diagnostics_count(...)` and
  `line_drawing_app_stage_diagnostic_at(...)`.

## Boundary

- `src/main.c` should stay a thin executable entry that delegates into
  `line_drawing_app_main(...)`.
- This wrapper owns lifecycle sequencing and diagnostics; the host menu/editor
  runtime remains in the legacy runtime session behind the run-loop handoff.
- Stage names are wrapper checkpoints, not ownership transfers for product
  configuration:
  - Core `Global_Init(...)`, `data_paths`, and startup-config helpers own
    runtime data-root loading, fallback, and persistence.
  - Core recent contexts own recent layout/scene/object/input/output lists.
  - UI File-browser code owns persisted browser mode/root/entry restore.
  - The macOS launcher/package lane owns packaged runtime directory setup,
    MoltenVK ICD setup, and launcher log/config diagnostics.
- Do not add product behavior, File-pane logic, CAD tools, scene export policy,
  or mesh import behavior here. Add those to the owning source lane and keep
  this layer focused on app startup/shutdown composition.
