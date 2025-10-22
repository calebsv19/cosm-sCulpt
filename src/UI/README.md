# UI Module

The UI layer provides a lightweight button panel and shared font management.

## Files
- `ui_panel.h` / `ui_panel.c` — define `UIButton` and `UIPanelState`, construct a static panel with left/right button groups, and expose access to the panel state.
- `render_ui_panel.h` / `render_ui_panel.c` — draw buttons using SDL2 renderer primitives and SDL_ttf fonts. `DrawButton` handles background, border, and centred text.
- `input_ui_panel.h` / `input_ui_panel.c` — detect clicks inside button bounds before layout editing occurs. Executes actions such as saving/loading JSON, resetting the origin, zooming, toggling delete mode, and pinning the selected anchor.
- `font_manager.h` / `font_manager.c` — initialise SDL_ttf, load font resources from `include/fonts`, expose typed getters, and release fonts on shutdown.

## Workflow
- The mouse handler calls `UIPanel_HandleClick` first; returning `true` stops the editor from treating the click as a layout action.
- Rendering runs `Render_UIPanel` after world geometry so the panel sits on top of the scene.
- UI actions call back into the layout, grid, and editor modules, keeping UI-specific logic out of the core systems.
