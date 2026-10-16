# Shared UI preparation and parity

The persistent Main Edit lane is the functional owner. Preparation qualifies its
selected dirty source without adopting canonical drift, changing VERSION,
refreshing imports, modifying scene data, or replacing Desktop applications.
The current baseline uses imported kit_ui0.11.2, kit_render0.14.4 and
vk_renderer1.3.2. Existing theme/font policy is already shared. This baseline is
not a claim of parity with the latest workspace UI/render modules.

## Contained qualification

Run from Main Edit, using fresh absolute external roots:

```sh
python3 -B tools/verification/prepare_shared_ui.py \
  --shared-root /absolute/workspace/shared \
  --work-root /absolute/fresh-work-root \
  --report-root /absolute/fresh-report-root
```

The tool snapshots tracked and visible untracked source, records SHA-256 hashes,
excludes and preserves existing `.cache` and nested `Users` artifact trees,
freezes four workspace fallback modules plus the desktop target-contract helper,
and builds without cleaning retained outputs. Tests and native windows use
copied source/private cwd/config/font/assets. Source and input readback detects
changes to the selected input set. Task-owned output remains for caller cleanup
after consumer checks; failures retain logs and exact inputs.

Gates: fresh Clang build; direct input-policy and shared theme/font tests;
Main Edit package-contract checks; full host suite; agent-scene export/refusal
smokes; native menu/editor readbacks; validated Vulkan startup/resize/restart at
2x drawable scale. The first qualified preparation has611 cases in64 groups.
No installed-package or manual-input acceptance follows from this source proof.
Fullscreen, focus cancellation, hide/show and minimize/restore belong to the next
shared-window integration slice.

The historical `verify-vulkan-rollout.py` main contract pins shared commit60084f9
and renderer1.3.1. It remains a historical exact contract. The new route records
the actual selected source/import identities and reuses its native validation
layer setup and BMP checks; native C verifies startup/resize/restart receipts.
`test-legacy` suppresses theme-test failure, so qualification calls the real theme
test target directly. The legacy subtree manifest still specifies `make clean`;
do not run that verify command on retained source. The contained route supplies
the qualification gate for a future reviewed managed import.
The Git-free snapshot may log a nonfatal legacy Make metadata lookup error;
source identity is recorded from the original registered checkout separately.

## Next integration boundary

Select a committed UI/render dependency closure and qualify its parity before
shared window integration. kit_ui0.18.0 and kit_render0.15.0 are candidates.
Workspace renderer1.8.0 includes unrelated uncommitted depth work; it must not be
silently adopted. The committed renderer is1.7.0. Reuse native Vulkan presentation
and preserve scene, camera, mesh preparation, canvas, history, saves and drafts.
Preserve both source histories (18 Main Edit-only and2 canonical-only commits);
review release/source overlap before later canonical adoption. No blanket corner
rounding or canvas gesture rewrite is part of the UI rollout.

## Foundation continuation — 2026-10-15

The next integration boundary above is now qualified in Main Edit. Add
`--lifecycle` to the preparation command for shared native window probes and
document/history readback. The existing historical baseline text remains dated
preparation evidence; see [current foundation](ui_window_foundation.md).
