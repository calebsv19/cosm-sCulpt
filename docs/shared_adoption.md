# Shared input admission and build identity

The ordinary source build selects `third_party/codework_shared`. The adoption
manifest binds 467 files across 33 selected modules to the accepted vendor
snapshot at `d7670cd`. Module versions and imported implementations are unchanged.
Missing vendored scene-view, viewport, picking or viewport-kit headers no longer
silently select a workspace copy. An explicit alternative root must match the
same module inputs. The verifier refuses missing, changed, nonordinary or new
source files before dependent object recipes, including otherwise reusable builds.

The application directly compiles shared sources into its object tree. Compiler
commands, compiler version, flags, root selection, Make rules, manifest and
verifier contents determine a stable build-identity stamp. Object families depend
on this stamp; unchanged identity keeps its timestamp. Existing native dependency
files track transitive includes. The ordinary Clang route retains its original
Make dependency emission. The fisiCs route uses `tools/fisics_compile.py` to publish Make dependencies
from the compiler's actual include graph. Failed compilation or unavailable/invalid
graphs preserve the previous object and dependency pair. The compiler executable
and adapter bytes also participate in build identity. This qualifies incremental
dependency handling separately from full compiler/consumer behavior.

Use a fresh disposable source copy and the workspace target-contract helper's
absolute path. `clang-build` and `fisics-build` still invoke clean and are not
verification entrypoints for retained worktrees. `make all` selects the ordinary
route without cleanup. Explicit compiler/SDK selection is part of verification.

The disposable test accepts `--disposable-root`, `--workspace-root`, and
`--compiler`; it mutates fixture headers/rules and verifies actual object reuse,
invalidation and input refusal. Never point it at a retained checkout.

Package identity, native behavior, compiler/platform routes and installed or
release states require their separate evidence. No VERSION, shared source,
release or user data mutation is implied by this consumer policy.
