# Line Drawing Docs Index

Start here for public repository documentation.

## Scaffold State
- `docs/current_truth.md`: current scaffold/runtime structure and verification snapshot.
- `docs/future_intent.md`: planned scaffold convergence and remaining migration slices.
- `docs/desktop_packaging.md`: `.app` packaging commands, launcher diagnostics, and Desktop refresh workflow.

Current verification contract:
- `make -C line_drawing clean && make -C line_drawing`
- `make -C line_drawing run-headless-smoke`
- `make -C line_drawing visual-harness`
- `make -C line_drawing test-stable`
- `make -C line_drawing test-legacy`
- `make -C line_drawing scene-pipeline-smoke`
- `make -C line_drawing package-desktop`
- `make -C line_drawing package-desktop-self-test`
- `make -C line_drawing package-desktop-refresh`

## Public Runtime Docs
- `README.md` (repo root): product/runtime overview and build/run flow.
- `docs/KEYBINDINGS.md`: current runtime keybind reference.

## Private Planning Docs
- Private scaffold plans and internal execution docs are in the workspace private docs bucket:
  - `../docs/private_program_docs/line_drawing/`
