# Sculpts / LineDrawing Agent Guide

## Repository layout

- `src/`: C application code, organized by subsystem; CLI tools are in `src/Tools/`.
- `include/`: public headers and fonts; `tests/`: unit and integration tests.
- `config/`: runtime layouts; `export/`: generated assets.
- `third_party/codework_shared/`: vendored shared modules; `external/cjson/`: JSON dependency.

## Local commands

- `make`: build `build/toolchains/clang/bin/LineDrawing`.
- `make test`: build and run the host unit tests.
- `make agent_scene_tool`: build the agent scene CLI.
- `make agent-scene-smoke`: verify the agent scene producer.
- `make shape_tool` and `make shape-sanity`: build and check shape tooling.

Use the tests relevant to the change. Preserve existing build outputs unless a
clean rebuild is needed. Launch the desktop app only when the task calls for it.

## Implementation

Use C11, four-space indentation and the existing naming conventions:
`snake_case` files and `Module_Action` public functions. Keep builds warning-clean
under `-Wall -Wextra -Werror -Wpedantic` and use module-qualified header paths.

Add brief comments where they explain a public contract, ownership rule or
non-obvious behavior. Update the relevant README or contract when behavior changes.
Use the existing test framework and register new C suites in `tests/test_runner.c`;
use the CLI integration tests for producer/export behavior.

## Changes and commits

Check `git status --short` before editing and preserve unrelated work. Make focused
local commits for completed, verified implementation work unless the user asks to
leave it uncommitted. Do not request confirmation again for an authorized commit.
Keep publishing, releases and deployment within the user's explicitly requested scope.

Use concise commit titles. PR descriptions should explain the behavior change and
relevant validation; include captures when they help review a UI change.
