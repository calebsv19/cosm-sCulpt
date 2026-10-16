# Shared launcher and editor shell controls

The retained Main Edit lane adapts committed kit_ui 0.18.0 interaction and surface
snapshots. App-owned actions, filesystem I/O, canvas gestures, camera, history and
save/draft policies retain their existing implementations. Shared module source
and versions are unchanged by this integration.

`UI/shared_controls` adapts logical SDL events and renders shared pressed/focus
state using the Vulkan drawing facade and existing theme palette. Real SDL event
conversion compiles outside that drawing facade. `shared_editor_controls` owns
editor snapshots; `Menu/line_drawing_host_menu_controls` owns launcher snapshots.
The existing shell and topbar expose their measured actionable rectangles.

Launcher rows use path identity and section scope, with clipped visible bounds;
row replacement cannot transfer a held press. Navigation and folder actions use
semantic action keys. Unavailable reopen/history/saved-camera controls cannot
activate. Capture is canceled on runtime initialization, native window changes,
host transitions and modal/surface changes. An empty or invalid snapshot admits
no action. Rectangular painting and existing layout dimensions are retained.

Editor controls support pointer release, Tab/Shift-Tab and Enter/Space release.
Launcher region/arrow/filter navigation remains local; content actions complete
on key release. Text owners keep their keyboard events. Scrollbars and pane
splitters keep their dedicated drag owners. Escape dismisses open popup/context
menus before camera exit or editor return. Visibility-tree internals, file-browser
lists, specialized panel controls and text editors are later slices.

Verification uses fresh task-owned source/build inputs, dependency-tracked delta
rebuilds after repairs, `make test ARGS=SharedControls`, direct input/theme checks,
full `make test`, generic kit_ui tests, package static contracts and agent export
smoke. `tools/verification/prepare_shared_ui.py --lifecycle` documents the contained
native Vulkan startup/resize/restart and menu/editor window gates. Current proof:
617 cases/66 groups, four focused integration cases, all native stages pass with
validation, preserved document/history/saved state, and initial menu/editor image
hash equality with the foundation checkpoint. This is source/runtime proof;
installed packages and manual OS interaction acceptance remain separate.
