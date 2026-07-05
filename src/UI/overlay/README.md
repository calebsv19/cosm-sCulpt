# UI Overlay

This subtree owns modal and overlay rendering above the pane-clipped editor UI.

## Ownership

- `ui_panel_overlay_render.c` — overlay shell/chrome composition.
- `ui_panel_overlay_dialog_common.c` — shared dialog render helpers.
- `ui_panel_overlay_dialog_file.c` — file/root dialog rendering.
- `ui_panel_overlay_dialog_edit.c` — edit/dimension/bounds dialog rendering.

## Boundary

- JSON/scene/STL browser list ownership belongs to the persistent File pane,
  not this overlay lane.
- Dialog apply/cancel behavior lives in panel dialog logic; this subtree should
  stay focused on overlay rendering and modal presentation.
