# sCulpt text and style transactions

The three fixed font slots prepare completely before replacement. A failed load
preserves all active handles and rolls semantic preset/zoom back without reopening
working fonts. Font zoom keeps the existing -2 through +4 range and role-size
policy. Both shared presets and the existing legacy-font toggle remain available.

Entering authoring pins the entry bank lazily on first replacement. Cancel,
toggle-off and normal shutdown restore exact entry handles without opening fonts.
Apply writes font preset, zoom and theme together to `data/runtime/ui_style.txt`
through `core_io_write_all_atomic` before accepting the draft. Failed writes leave
the overlay open and the old file unchanged. The versioned bounded record is
validated as a whole. When absent, the prior font/zoom and theme files remain
readable; new saves use the combined record. No scene, document, camera or history
ownership moves into this preferences file.

Layout measurements and drawing share the registered renderer's actual density.
KitRender 0.16.0 supplies native-pixel Vulkan text. SDL uses integer device-pixel
painting and restores caller scale, viewport and clip. Missing glyph runs use the
shared macOS native-font adapter and a bounded app-owned Vulkan fallback cache;
texture retirement stays behind submitted frames. Long editable fields retain
selection/preedit and scroll the composition into view beside its caret.

The exact scoped module pin is `tools/text-render-source-pin.json`. Only KitRender
was imported from committed shared 683068aa3c89c3570f719880b12f738c23d4b3d5;
other modules and older full-tree pins retain their existing identities. The legacy
whole-renderer rollout verifier still describes its older historical checkpoint.
Use `test-text-source` for this module's exact committed-source check.

Qualify in fresh external outputs, passing the workspace target helper and shared
fallback root explicitly. `make all build-text-proof`, `make test`,
`make agent-scene-smoke` and the Main Edit package self-test are the source gates.
`make test` includes the font/settings fault and separate-process reopen corpus;
its runner owns disposable preference files. Native pixel proofs are standalone
executables with font and output-directory arguments; Vulkan requires a native
display and absolute-path validation-layer manifest. No clean target is needed.

The uncommitted checkpoint is in registered Main Edit over 33674c0 / VERSION 0.7.0.
Canonical 9e7f92e / VERSION 0.8.0, installed apps and release records are separate.
Native Linux rendering and human multistage IME acceptance are not established by
the macOS qualification. Whole-application responsive redesign remains separate.
