# Core Module

The core layer owns application-wide state and the SDL boilerplate so other systems can stay focused on layout editing.

## Files
- `global_state.h` / `global_state.c` — allocate and expose the singleton `GlobalState`, initialise the grid, layout, editor, and UI panel, and provide helpers such as `Global_GetScreenWidth`. Dirty flags (`layoutDirty`, `layoutDirtySinceSave`, `hitboxDirty`) plus the helpers `Global_FlagLayoutChanged`, `Global_FlagGridChanged`, and `Global_RebuildHitboxesIfDirty` keep hitboxes in sync while avoiding unnecessary rebuilds. Persistence helpers (`Global_OnLayoutSaved`, `Global_OnLayoutLoaded`, `Global_GetCurrentConfigPath`, `Global_IsLayoutDirty`) track the active layout file and whether the in-memory state has diverged from disk. `SpaceMode` state (`2D`/`3D`) is now owned here, including startup persistence via `config/space_mode.txt` and mode-constraint enforcement.
- `line_drawing_pane_host.h` / `line_drawing_pane_host.c` — app-local pane-host seam built on shared `core_layout`, `core_pane`, and `core_pane_module`. Seeds the canonical 4-lane UI graph (`top bar`, `left controls`, `center canvas`, `right controls`), validates/solves leaf rectangles, and validates role module bindings for deterministic pane geometry ownership.
- `space_mode_adapter.h` / `adapters/space_mode_adapter.c` — central mode-aware adapter seam for projection/view context. It builds a single `SpaceViewContext` that normalizes runtime behavior (`2D` lock or `3D` free-view) and exposes shared helpers for projection and screen-to-world conversion used across input/editor/layout/render paths.
- `SDLApp/` — SDL wrapper providing the runtime loop with mode-aware wait policy, render-on-dirty heartbeat behavior, and loop diagnostics emission (see `SDLApp/README.md` for details).

## Interactions
- Input, rendering, layout, and UI code call `Global_Get()` to access shared state instead of passing references around.
- Window resize events captured in the SDL loop call `Global_SetWindowSize`, which keeps the UI and grid aware of the current renderer size.
- Systems that mutate layout geometry call `Global_FlagLayoutChanged`, while view/navigation code calls `Global_FlagGridChanged`; both funnel into `Global_RebuildHitboxesIfDirty`, which compacts layout deletions on demand before regenerating selection hitboxes.
- Window-size updates now also rebuild the pane-host solve so pane leaf rectangles stay synchronized with runtime bounds.
- Save/load UI paths call `Global_OnLayoutSaved` / `Global_OnLayoutLoaded` so the info overlay and undo history know which config file is in play and whether a rename prompt should appear.
- `Global_Shutdown` releases layout resources on exit, while the SDLApp layer tears down the SDL renderer/window.
