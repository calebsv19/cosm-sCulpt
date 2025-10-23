# Core Module

The core layer owns application-wide state and the SDL boilerplate so other systems can stay focused on layout editing.

## Files
- `global_state.h` / `global_state.c` — allocate and expose the singleton `GlobalState`, initialise the grid, layout, editor, and UI panel, and provide helpers such as `Global_GetScreenWidth`. Dirty flags (`layoutDirty`, `hitboxDirty`) plus the helpers `Global_FlagLayoutChanged`, `Global_FlagGridChanged`, and `Global_RebuildHitboxesIfDirty` keep hitboxes in sync while avoiding unnecessary rebuilds.
- `SDLApp/` — SDL wrapper providing the main loop and render throttling (see `SDLApp/README.md` for details).

## Interactions
- Input, rendering, layout, and UI code call `Global_Get()` to access shared state instead of passing references around.
- Window resize events captured in the SDL loop call `Global_SetWindowSize`, which keeps the UI and grid aware of the current renderer size.
- Systems that mutate layout geometry call `Global_FlagLayoutChanged`, while view/navigation code calls `Global_FlagGridChanged`; both funnel into `Global_RebuildHitboxesIfDirty`, which compacts layout deletions on demand before regenerating selection hitboxes.
- `Global_Shutdown` releases layout resources on exit, while the SDLApp layer tears down the SDL renderer/window.
