# UI Module

The UI layer provides a lightweight button panel and shared font management.

- `ui_panel.h` / `panel/ui_panel.c` — define `UIButton` and `UIPanelState`, own pane-aware control metadata, construct the left/right grouped control surface, keep a cache of discovered `config/*.json` files, and coordinate panel relayout against the current pane-host solve.
- `render_ui_panel.h` / `panel/render_ui_panel.c` — draw pane-scoped buttons/section chrome using SDL2 renderer primitives and SDL_ttf fonts. `DrawButton` handles background, border, and centred text, while root-summary text is clipped/ellipsized to avoid overflow.
- `input_ui_panel.h` / `panel/input_ui_panel.c` — detect clicks inside button bounds before layout editing occurs. Executes actions such as saving/loading JSON (modal filename prompt + dropdown), root-path edits/folder picks, resetting the origin, zooming, toggling delete mode, pinning the selected anchor, toggling object center-gizmo mode (`Move`/`Rotate`), and toggling `SpaceMode` (`2D`/`3D`), and raises the appropriate dirty flags so hitboxes/layout stay in sync.
- `panel/ui_panel_controls.c` — declare and seed the grouped button/control set used by the pane-scoped right/left sidebars.
- `panel/ui_panel_dialog_logic.c` — typed dialog apply/cancel logic for dimensions, bounds, offsets, and selected-object transform editing.
- `panel/ui_panel_file_ops.c` — save/load/export and root-path file-operation helpers. It now owns both `Export Shape` and `Export Scene`, with the latter writing `scene_authoring.json` plus compiled `scene_runtime.json` into a named directory under the configured output root.
- `panel/ui_panel_input.c` — panel-local keyboard/text-entry helpers layered under the UI panel state machine.
- `panel/ui_panel_load_menu.c` — cached load-menu and config dropdown helpers.
- `overlay/ui_panel_overlay_render.c` — draw panel overlays such as dropdowns and modal dialog chrome above the pane-clipped base panel render path.
- `info_overlay.h` / `info_overlay.c` — render a top-of-screen status bar displaying details about the current selection, hover target, drag mode (snapped vs precise), bezier handle lengths/angles, multi-selection counts, current file name, active `SpaceMode`, active view/plane context, dirty state, and undo/redo depth.
- `font_manager.h` / `font_manager.c` — initialise SDL_ttf, load font resources from `include/fonts`, expose typed getters, and release fonts on shutdown.

## Workflow
- The mouse handler routes by pane first, then calls `UIPanel_HandleClick`; returning `true` stops the editor from treating the click as a layout action. Motion events are also forwarded so the load dropdown can provide hover feedback.
- Rendering runs `Render_InfoOverlay` (status bar) followed by pane-clipped `Render_UIPanelSide(...)` / `Render_UIPanelRootSummary(...)` work and finally `UIPanel_RenderOverlays`, which draws the dropdown plus save/root modals on top of everything else.
- UI actions call back into the layout, grid, editor, and tooling modules, keeping UI-specific logic out of the core systems while `Global_OnLayoutSaved/Loaded` keep the dirty flag and filename metadata in sync. The link-handles button mirrors the `L` keyboard shortcut, the pin button mirrors `P`, `Export Scene` routes through `Tools/scene_export.c`, and the overlay/marquee visuals ensure the user can see multi-selection and drag state even when the mouse is off-screen.
