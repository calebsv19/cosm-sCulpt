# SDLApp Framework

This submodule wraps raw SDL2 setup and the main loop in a reusable interface.

## Files
- `sdl_app_framework.h` — declares `AppContext`, render mode options, lifecycle functions (`App_Init`, `App_Run`, `App_SetRenderMode`, `App_Shutdown`), and the callback structure consumed by the main loop.
- `sdl_app_framework.c` — implements the framework: initialises SDL, creates the window/renderer, tracks delta time, handles window-resize notifications (`Global_SetWindowSize`), and runs a wait-aware event loop with render-on-dirty with explicit timed updates.
- `sdl_app_loop_policy.h` / `sdl_app_loop_policy.c` — wait policy for settled, dirty, timed, suspended, and failed-render states.
- `sdl_app_loop_diag.h` / `sdl_app_loop_diag.c` — schema-locked `LoopDiag` sink for blocked-vs-active calibration (`LINE_DRAWING_LOOP_DIAG_*` env controls).

## Integration
- `main.c` constructs an `AppContext`, sets demand rendering with a 1/60-second initial update delta, and hands function pointers to `App_Run`.
- Input callbacks receive SDL events from wait+poll intake; update callbacks run once per wake; render callbacks run on input/invalidation or an actual visible deadline. `nextUpdateDelayMs` returns -1 when settled, 0 when due, or a positive delay. `redraw_requested` lets an update explicitly invalidate the frame.
- Settled waits block in SDL for up to 1000 ms, interrupted immediately by input. The maintenance wake does not draw. Playback requests 16 ms ticks; imports request 50 ms progress polls; preview quality and status expiry request their own deadlines. Hidden/minimized windows retain dirty state without drawing, and failed presentation retries wait for a bounded interval (50 ms by default). There is no idle redraw heartbeat.
- The app-specific cache contracts and validation evidence are in [demand_rendering.md](../../../docs/demand_rendering.md).
- When the window closes or `AppContext.quit` is set, `App_Shutdown` frees the renderer/window and calls `SDL_Quit`.
- The existing visual-artifact lane can stage a real canonical runtime mesh by
  setting `LINE_DRAWING_VISUAL_MESH_RUNTIME` alongside
  `LINE_DRAWING_VISUAL_ARTIFACT`; it normalizes the mesh only in the temporary
  proof scene, warms the adaptive preview into settled quality, and captures the
  initialized renderer without changing normal startup behavior. Set
  `LINE_DRAWING_VISUAL_PREVIEW_MODE` to `bounds`, `wire`, or `material` to prove
  a non-default preview representation; unset values retain the flat default.
- Mesh surface post-processing uses optional shared `kit_viewport3d >= 0.1.0`
  for stable object accents and silhouette/depth/object-owner outline roles.
  The SDL app still owns projection, CPU raster/cache quality, buffer lifetime,
  upload/drawing, picking, overlays, and all input/authoring arbitration.
