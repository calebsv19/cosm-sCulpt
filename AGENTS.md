# Repository Guidelines

## Project Structure & Module Organization
Core source lives under `src/` and is split by subsystem: `Core/`, `Input/`, `Layout/`, `Render/`, `UI/`, `Editor/`, `Math/`, and tooling in `src/Tools/` (`ShapeLib` plus export/trace helpers).  
Tests live in `tests/` (`test_runner.c`, `test_math.c`, `test_layout.c`).  
Runtime layout JSON files are in `config/`; exported shape assets are written to `export/`; font assets are in `include/fonts/`; third-party code is in `external/cjson/`.

## Build, Test, and Development Commands
- `make`: Build app binary at `build/bin/LineDrawing`.
- `make run`: Build and launch the SDL app.
- `make DEBUG=1`: Build with debug symbols (`-O0 -g`).
- `make test`: Build and run `build/tests/bin/run_tests`.
- `make shape_tool`: Build CLI shape conversion tool.
- `make shape-sanity`: Build shape sanity utility.
- `make export-assets SHAPE_ASSET_DIR=...`: Sync exported shapes into canonical shared asset directory.
- `make clean`: Remove `build/`.

## Coding Style & Naming Conventions
Use C11 and keep code warning-clean under `-Wall -Wextra -Werror -Wpedantic`.  
Follow existing conventions:
- 4-space indentation, braces on same line for functions/control flow.
- File names use `snake_case` (for example `layout_json.c`).
- Public API functions use `Module_Action` style (`Layout_AddWall`, `Global_Init`).
- Keep headers paired with implementation files and include project headers by module path (`"Layout/layout.h"`).
- Make single to double line summary comments for each method or important strtcu enum etc, so that all parts of our code files have easily referenceable summaries for each method and symbol.
- After any major coding changes and updates to our system updates the read me docs in all given areas that have been edited or modified file wise.

## Testing Guidelines
Add/extend tests in `tests/test_*.c` and wire suites through `tests/test_runner.c`.  
Use the lightweight framework in `tests/test_framework.h` (`TEST_ASSERT`, grouped cases via `run_test_cases`).  
Cover layout JSON compatibility, editor undo/redo behavior, and math/grid helpers when touching those paths.

## Commit & Pull Request Guidelines
Match repository history style: short, imperative/past-tense summaries with a capitalized first word (for example, `Added Export Button`, `Updated Makefile`).  
For PRs, include:
- What changed and why.
- Commands run (`make`, `make test`).
- Screenshots/video for UI or rendering changes.
- Linked issue/task when applicable.

URGENT NOTE FOR GIT COMMITS
ONLY MAKE COMMIT AFTER EXPLICIT PERMISSION IS ASKED AND GIVEN BY THE USER, SO THAT NO UNNECCESSARY COMMITS ARE MADE

## Environment & Dependency Notes
The `makefile` now resolves shared modules through the vendored `third_party/codework_shared` subtree (for example `third_party/codework_shared/vk_renderer`, `third_party/codework_shared/core/core_base`, `third_party/codework_shared/timer_hud`).
