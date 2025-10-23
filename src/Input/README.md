# Input Module

The input layer translates SDL events into grid camera movement, layout edits, and editor state updates.

## Files
- `input_handler.h` / `input_handler.c` — central dispatcher invoked from `main.c`. It asks `Global_RebuildHitboxesIfDirty()` for fresh hitboxes before forwarding events to the device-specific handlers.
- `input_mouse.h` / `input_mouse.c` — handles mouse wheel zoom, left-click selection (anchors take priority over walls), right-click wall placement via `Editor_ClickAt`, and drag-to-pan behaviour. It also calls `UIPanel_HandleClick` so UI interactions short-circuit layout edits, and raises `Global_FlagGridChanged` whenever panning/zooming alters the camera.
- `input_keyboard.h` / `input_keyboard.c` — polls held keys for continuous panning and zoom, toggles the global quit flag on `Esc`, and reacts to discrete key presses: `Shift` constrains wall placement, `D` toggles delete modes, `O` recentres the grid on the selected anchor, `P` toggles anchor persistence, and `Delete/Backspace` remove the selected wall or anchor. `Ctrl/Cmd+Z` performs undo, `Ctrl/Cmd+Shift+Z` or `Ctrl/Cmd+Y` performs redo, and grid navigation updates flag the hitboxes as dirty to keep selection accurate.

## Interactions
- Mouse and keyboard handlers depend on `Global_Get()` to access the grid, layout, and editor.
- `Global_RebuildHitboxesIfDirty` centralises hitbox regeneration so the input layer never runs stale selection queries.
- UI button actions triggered from `UIPanel_HandleClick` call into layout persistence, grid zoom helpers, and editor mode toggles (raising the relevant dirty flags as needed) and now capture undo snapshots before layout-changing operations.
