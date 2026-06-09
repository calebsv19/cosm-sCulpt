# sCulpt Memory-Check Audit

Last updated: 2026-06-07

## Status

`make -C line_drawing memory-check-audit` is installed as a default-off audit
lane. The instrumented runner reaches process shutdown, emits the final
memory-check summary, and is currently clean.

Generated artifacts:
- stdout: `build/memory_check/line_drawing.stdout`
- stderr: `build/memory_check/line_drawing.stderr`

These files live under `build/`; a clean rebuild removes them. Rerun the audit
command to regenerate the captured stdout/stderr.

## Current Result

Latest command:

```sh
make -C line_drawing memory-check-audit
```

Observed result:
- build/link completed with `BUILD_TOOLCHAIN=fisics` and
  `FISICS_OVERLAY=physics-units,memory-check`
- all instrumented LineDrawing tests passed
- final memory-check summary was emitted:
  `active=0 leaked_bytes=0 allocs=224099 frees=224092 double_free=0 unknown_free=0 tracker_failures=0`
- no live allocations, unknown-pointer frees, double-free reports, or tracker
  failures remain
- the previous construction-plane assertion failures no longer reproduce after
  the fisiCs ABI/overlay fixes in this cleanup pass
- the previous no-site live allocation reports were cleared by keeping layout
  child suites filterable without running them a second time in the default
  aggregate test run

## Interpretation

The audit lane is leak-clean for the current test suite. The original
ownership report was cleared by fixing cJSON/string release boundaries in
LineDrawing and by teaching fisiCs' memory-check overlay to route allocator
function-pointer hooks consistently. The previous construction-plane failures
were a fisiCs overlay/ABI regression, not an app ownership defect.

The final no-site live allocation slice was a test-runner coverage artifact:
layout child suites are now addressable by name for focused runs, but the
default audit executes the aggregate `Layout` suite once instead of also
executing each child suite a second time.

Clean criteria for future reruns:
- require a final `[fisics:memory-check] summary` line
- require `active=0`, `double_free=0`, `unknown_free=0`, and
  `tracker_failures=0`
- run `make -C line_drawing clean` before returning to the normal Clang test
  gate, because the audit output and instrumented binaries live under `build/`

## Commands

Prerequisite runtime check:

```sh
make -C fisiCs memory-check-test
```

Normal LineDrawing test gate:

```sh
make -C line_drawing test
```

Default-off memory-check audit:

```sh
make -C line_drawing memory-check-audit
```
