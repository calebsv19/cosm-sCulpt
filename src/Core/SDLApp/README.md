# SDLApp Framework

This submodule wraps raw SDL2 setup and the main loop in a reusable interface.

## Files
- `sdl_app_framework.h` — declares `AppContext`, render mode options, lifecycle functions (`App_Init`, `App_Run`, `App_SetRenderMode`, `App_Shutdown`), and the callback structure consumed by the main loop.
- `sdl_app_framework.c` — implements the framework: initialises SDL, creates the window/renderer, tracks delta time, handles window-resize notifications (`Global_SetWindowSize`), and runs a wait-aware event loop with render-on-dirty plus heartbeat behavior.
- `sdl_app_loop_policy.h` / `sdl_app_loop_policy.c` — mode-aware wait timeout policy (`idle`, `interaction-active`, `high-intensity`) used by the SDL framework loop.
- `sdl_app_loop_diag.h` / `sdl_app_loop_diag.c` — schema-locked `LoopDiag` sink for blocked-vs-active calibration (`LINE_DRAWING_LOOP_DIAG_*` env controls).

## Integration
- `main.c` constructs an `AppContext`, sets the render mode to throttled 60 FPS, and hands function pointers to `App_Run`.
- Input callbacks receive SDL events from wait+poll intake; update callbacks run once per loop; render callbacks run on dirty frames, interaction cadence, or bounded heartbeat.
- When the window closes or `AppContext.quit` is set, `App_Shutdown` frees the renderer/window and calls `SDL_Quit`.
