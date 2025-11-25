# Input Module

The input layer translates SDL events into grid camera movement, layout edits, selection workflows (click, marquee, and multi-anchor drag), and editor state updates.

## Files
- `input_handler.h` / `input_handler.c` — central dispatcher invoked from `main.c`. It asks `Global_RebuildHitboxesIfDirty()` for fresh hitboxes before forwarding events to the device-specific handlers and routes `SDL_TEXTINPUT` / mouse motion into the UI layer so the save-dialog text box and load dropdown can react.
- `input_mouse.h` / `input_mouse.c` — handles mouse wheel zoom, lasso selection, shift-click additive selection, double-click collapse of selection groups, left-drag group movement (with Alt overriding snapping for the primary anchor), right-click wall placement via `Editor_ClickAt`, and drag-to-pan behaviour. It also calls `UIPanel_HandleClick` so UI interactions short-circuit layout edits, updates hover highlights, and raises `Global_FlagGridChanged` whenever panning/zooming alters the camera.
- `input_keyboard.h` / `input_keyboard.c` — polls held keys for continuous panning and zoom, toggles the global quit flag on `Esc`, and reacts to discrete key presses: `Shift` constrains wall placement (and signals shift state to the editor for marquee activation), `D` toggles delete modes, `O` recentres the grid on the selected anchor, `P` toggles anchor persistence, `C` converts anchors between corner/curve, `L` toggles linked handles, and `Delete/Backspace` remove the selected wall or anchor. `Ctrl/Cmd+Z` performs undo, `Ctrl/Cmd+Shift+Z` or `Ctrl/Cmd+Y` performs redo, and the handler defers to the panel when the save-dialog is capturing keyboard input.

## Interactions
- Mouse and keyboard handlers depend on `Global_Get()` to access the grid, layout, and editor. The mouse path now mirrors editor functionality: shift starts marquee selection, Alt overrides snapping during drags, and multi-selection data is stored in `EditorState`.
- `Global_RebuildHitboxesIfDirty` centralises hitbox regeneration so the input layer never runs stale selection or handle hit-tests.
- UI button actions triggered from `UIPanel_HandleClick` call into layout persistence, grid zoom helpers, editor mode toggles, and history capture (raising the relevant dirty flags as needed). The save dialog / load dropdown continue to consume input so accidental layout edits are avoided while typing or choosing configs.
