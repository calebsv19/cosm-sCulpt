# Line Drawing Future Intent

Last updated: 2026-04-05

## Scaffold Alignment Intent
1. Keep existing 2D/3D parity behavior unchanged while normalizing scaffold contracts.
2. Standardize docs + verification + lifecycle stage wrappers to match other migrated programs.
3. Keep runtime mutable state in ignored lanes (`data/runtime/`) with fallback compatibility.
4. Preserve private/public docs separation policy.

## Planned Structural Intent
- `LD-S0` (completed):
  - baseline freeze captured (dirty working tree acknowledged and mapped before scaffold edits)

- `LD-S1` (completed):
  - added public scaffold docs:
    - `docs/current_truth.md`
    - `docs/future_intent.md`
    - `docs/README.md`

- `LD-S2` (completed):
  - added scaffold verification aliases:
    - `run-headless-smoke`
    - `visual-harness`
    - `test-stable`
    - `test-legacy`

- `LD-S3` (completed):
  - added canonical wrapper entry API:
    - `include/line_drawing/line_drawing_app_main.h`
    - `src/app/line_drawing_app_main.c`
  - locked lifecycle stage symbols and routed `main()` through `line_drawing_app_main(...)`

- `LD-S4` (completed):
  - locked runtime/temp ignore policy in `.gitignore` (`tmp/`, `data/runtime/`, `data/snapshots/`, `ide_files/`)
  - moved runtime persistence targets to `data/runtime/*` with fallback compatibility from legacy paths

- `LD-S5` (completed):
  - run full verification sweep and sync closeout docs:
    - private plan + private index
    - `line_drawing/docs/current_truth.md`
    - global scaffold matrix/backlog trackers
  - closeout commit after user confirmation with exact title:
    - `Project Scaffold Standardization`
  - verification snapshot (2026-04-02):
    - `make -C line_drawing clean && make -C line_drawing` pass
    - `make -C line_drawing run-headless-smoke` pass
    - `make -C line_drawing visual-harness` pass
    - `make -C line_drawing test-stable` pass
    - `make -C line_drawing test-legacy` keeps expected quarantined shared-theme adapter failure lane

## Desktop Packaging Intent
- packaging baseline is complete (`LD-PK0` through `LD-PK2`) with standardized `package-desktop*` targets and launcher diagnostics.
- release-readiness lane is now active:
  - `RL0` complete:
    - release contract (`release-*` targets), canonical product/bundle/version fields
    - package output aligned to canonical app name (`sketCh.app`)
  - `RL1` complete:
    - bundled Vulkan portability contract (`libMoltenVK.dylib`, `libvulkan.1.dylib`)
    - runtime ICD env contract (`VK_ICD_FILENAMES`, `VK_DRIVER_FILES`)
    - runtime shader root in writable runtime lane
    - ad-hoc signature hardening after local install-name mutations
  - next:
    - `RL2` complete: Developer ID signing + notarization + staple + verify-notarized
    - `RL3-RL5` remain: final desktop manual pass + release-closeout docs/memory/commit sync

## Connection Pass Intent
- baseline complete:
  - `LD-CP0` baseline routing/ownership map captured
  - `LD-CP1` stage/context ownership lock landed in wrapper
  - `LD-CP2` explicit runtime dispatch seam landed with typed request/outcome contract
  - execution note:
    - `../docs/private_program_docs/line_drawing/2026-04-02_line_drawing_connection_pass_cp0_cp2_execution.md`
- next:
  - optional `LD-CP3+` extraction only if deeper wrapper/runtime ownership split is needed

## Cross-Program Wrapper Initiative
- `W0` complete (canonical wrapper contract frozen)
- `W1` complete for `line_drawing` (typed wrapper context + dispatch seam alignment)
- `W2` complete for `line_drawing` (structured wrapper diagnostics and final wrapper summary logging)
- `W3` complete for `line_drawing`:
  - `S0` baseline freeze + verification rerun complete
  - `S1` typed runtime-loop adapter seam complete
  - `S2` typed run-loop handoff seam cutover complete
  - `S3` seam diagnostics + ownership hardening complete
  - `S4` closeout/docs/memory sync complete
- next:
  - optional `W4+` / `LD-CP3+` only if deeper wrapper/legacy ownership extraction is needed
- execution note:
  - `../docs/private_program_docs/line_drawing/2026-04-02_line_drawing_w1_w2_wrapper_hardening.md`
  - `../docs/private_program_docs/line_drawing/2026-04-02_line_drawing_w3_s0_s4_execution.md`

## IR1 Input Routing Intent
- baseline setup:
  - `IR1-S0` complete (top-level input map captured before edits)
  - `IR1-S1` complete (typed phase seam landed in `src/main.c`)
  - `IR1-S2` complete (explicit text-entry/global-shortcut precedence policy seam + policy test lane)
  - `IR1-S3` complete (diagnostics/tracker closeout)
  - execution note:
    - `../docs/private_program_docs/line_drawing/2026-04-03_line_drawing_ir1_s0_s1_execution.md`
    - `../docs/private_program_docs/line_drawing/2026-04-03_line_drawing_ir1_s2_policy_hardening.md`
    - `../docs/private_program_docs/line_drawing/2026-04-03_line_drawing_ir1_s3_closeout.md`
- next slices:
  - `IR1` lane complete for `line_drawing` (maintain-only)
  - queue `RS1` render-split lane when scheduled

## RS1 Render Split Intent
- baseline setup:
  - `RS1-S0` complete (top-level render ownership map captured)
  - `RS1-S1` complete (typed update/derive/submit seam landed)
  - `RS1-S2` complete (diagnostics/tracker closeout)
  - execution note:
    - `../docs/private_program_docs/line_drawing/2026-04-03_line_drawing_rs1_s0_s1_execution.md`
    - `../docs/private_program_docs/line_drawing/2026-04-03_line_drawing_rs1_s2_closeout.md`
- next slices:
  - `RS1` lane complete for `line_drawing` (maintain-only)
  - optional `RS1-S3+` only if deeper extraction is needed later

## Non-Goals During Scaffold Migration
- no new editor features
- no 2D/3D behavior redesign
- no broad header relocation sweep in this pass (kept as later follow-up lane)
