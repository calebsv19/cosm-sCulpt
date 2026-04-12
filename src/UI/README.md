# UI Module

The UI layer provides a lightweight button panel and shared font management.

- `ui_panel.h` / `ui_panel.c` — define `UIButton` and `UIPanelState`, construct a static panel with left/right button groups (save/load/export, input/output root edit + folder pick, reset origin, zoom, delete-mode toggle, pin anchor, link handles, primitive creation, prism dimension editors, display-unit cycling, object gizmo mode toggle `Move/Rotate`, scene-bounds controls (`Bounds`, `Clamp`, `Edit BMin`, `Edit BMax`), and `2D/3D` mode toggle), keep a cache of discovered `config/*.json` files, manage save-name/root-path/prism-dimension/scene-bounds dialogs, and expose helpers for load-menu/dropdown rendering plus native macOS folder selection.
- `render_ui_panel.h` / `render_ui_panel.c` — draw buttons using SDL2 renderer primitives and SDL_ttf fonts. `DrawButton` handles background, border, and centred text, while root-summary text is clipped/ellipsized to avoid overflow.
- `input_ui_panel.h` / `input_ui_panel.c` — detect clicks inside button bounds before layout editing occurs. Executes actions such as saving/loading JSON (modal filename prompt + dropdown), root-path edits/folder picks, resetting the origin, zooming, toggling delete mode, pinning the selected anchor, toggling object center-gizmo mode (`Move`/`Rotate`), and toggling `SpaceMode` (`2D`/`3D`), and raises the appropriate dirty flags so hitboxes/layout stay in sync.
- `info_overlay.h` / `info_overlay.c` — render a top-of-screen status bar displaying details about the current selection, hover target, drag mode (snapped vs precise), bezier handle lengths/angles, multi-selection counts, current file name, active `SpaceMode`, active view/plane context, dirty state, and undo/redo depth.
- `font_manager.h` / `font_manager.c` — initialise SDL_ttf, load font resources from `include/fonts`, expose typed getters, and release fonts on shutdown.

## Workflow
- The mouse handler calls `UIPanel_HandleClick` first; returning `true` stops the editor from treating the click as a layout action. Motion events are also forwarded so the load dropdown can provide hover feedback.
- Rendering runs `Render_InfoOverlay` (status bar) followed by `Render_UIPanel` and `UIPanel_RenderOverlays`, which draw the dropdown plus save/root modals on top of everything else.
- UI actions call back into the layout, grid, and editor modules, keeping UI-specific logic out of the core systems while `Global_OnLayoutSaved/Loaded` keep the dirty flag and filename metadata in sync. The link-handles button mirrors the `L` keyboard shortcut, the pin button mirrors `P`, and the overlay/marquee visuals ensure the user can see multi-selection and drag state even when the mouse is off-screen.
