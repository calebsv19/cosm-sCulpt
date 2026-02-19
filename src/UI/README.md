# UI Module

The UI layer provides a lightweight button panel and shared font management.

- `ui_panel.h` / `ui_panel.c` — define `UIButton` and `UIPanelState`, construct a static panel with left/right button groups (save/load, reset origin, zoom, delete-mode toggle, pin anchor, link handles), keep a cache of discovered `config/*.json` files, manage the save-name dialog, and expose helpers for load-menu/dropdown rendering. `Export Shape` now flattens layout data using the currently active edit plane (`XY/YZ/XZ`) to keep output aligned with the current slice workflow.
- `render_ui_panel.h` / `render_ui_panel.c` — draw buttons using SDL2 renderer primitives and SDL_ttf fonts. `DrawButton` handles background, border, and centred text.
- `input_ui_panel.h` / `input_ui_panel.c` — detect clicks inside button bounds before layout editing occurs. Executes actions such as saving/loading JSON (now via a modal filename prompt and dropdown), resetting the origin, zooming, toggling delete mode, and pinning the selected anchor, and raises the appropriate dirty flags so hitboxes/layout stay in sync.
- `info_overlay.h` / `info_overlay.c` — render a top-of-screen status bar displaying details about the current selection, hover target, drag mode (snapped vs precise), bezier handle lengths/angles, multi-selection counts, current file name, active view/plane context, dirty state, and undo/redo depth.
- `font_manager.h` / `font_manager.c` — initialise SDL_ttf, load font resources from `include/fonts`, expose typed getters, and release fonts on shutdown.

## Workflow
- The mouse handler calls `UIPanel_HandleClick` first; returning `true` stops the editor from treating the click as a layout action. Motion events are also forwarded so the load dropdown can provide hover feedback.
- Rendering runs `Render_InfoOverlay` (status bar) followed by `Render_UIPanel` and `UIPanel_RenderOverlays`, which draw the dropdown/save modal on top of everything else.
- UI actions call back into the layout, grid, and editor modules, keeping UI-specific logic out of the core systems while `Global_OnLayoutSaved/Loaded` keep the dirty flag and filename metadata in sync. The link-handles button mirrors the `L` keyboard shortcut, the pin button mirrors `P`, and the overlay/marquee visuals ensure the user can see multi-selection and drag state even when the mouse is off-screen.
