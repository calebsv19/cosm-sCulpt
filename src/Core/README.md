# Core Module

The core layer owns application-wide state and the SDL boilerplate so other systems can stay focused on layout editing.

## Files
- `global_state.h` / `global_state.c` — allocate and expose the singleton `GlobalState`, initialise the grid, layout, editor, and UI panel, and provide helpers such as `Global_GetScreenWidth`. `Global_TickSystems` runs every frame to compact deleted layout entities and rebuild wall/anchor hitboxes.
- `SDLApp/` — SDL wrapper providing the main loop and render throttling (see `SDLApp/README.md` for details).

## Interactions
- Input, rendering, layout, and UI code call `Global_Get()` to access shared state instead of passing references around.
- Window resize events captured in the SDL loop call `Global_SetWindowSize`, which keeps the UI and grid aware of the current renderer size.
- `Global_Shutdown` releases layout resources on exit, while the SDLApp layer tears down the SDL renderer/window.
