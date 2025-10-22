# SDLApp Framework

This submodule wraps raw SDL2 setup and the main loop in a reusable interface.

## Files
- `sdl_app_framework.h` — declares `AppContext`, render mode options, lifecycle functions (`App_Init`, `App_Run`, `App_SetRenderMode`, `App_Shutdown`), and the callback structure consumed by the main loop.
- `sdl_app_framework.c` — implements the framework: initialises SDL, creates the window/renderer, tracks delta time, handles window-resize notifications (`Global_SetWindowSize`), pumps events, and conditionally throttles rendering by tracking `timeSinceLastRender`.

## Integration
- `main.c` constructs an `AppContext`, sets the render mode to throttled 60 FPS, and hands function pointers to `App_Run`.
- Input callbacks receive every SDL event; update callbacks run once per loop to advance game state; render callbacks run either every iteration or at the configured cadence.
- When the window closes or `AppContext.quit` is set, `App_Shutdown` frees the renderer/window and calls `SDL_Quit`.
