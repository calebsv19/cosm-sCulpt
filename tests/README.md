# Tests

The test harness is a lightweight C executable that links against the engine code (everything except `src/main.c`). Each test file exposes a `*_run_tests` function, which the shared runner calls and reports on.

## Files
- `test_framework.h` / `.c` — minimal assertion macros plus `run_test_cases`, which prints per-group results.
- `test_math.c` — exercises `Math/math_util.h` helpers (snapping and coordinate conversions).
- `test_layout.c` — covers layout wall/anchor management, dirty-flag plumbing, JSON string round-trips, and undo/redo history behaviour.
- `test_runner.c` — entry point invoked by `make test`; aggregates all suites and emits a final pass/fail result.

## Running

```sh
make test
```

This command compiles the application sources into `build/obj`, builds the test objects under `build/tests/obj`, links them into `build/tests/bin/run_tests`, and runs the binary. Use `DEBUG=1` with the command to compile the entire stack with debug flags.
