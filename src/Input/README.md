# Input Module

The input layer translates SDL events into grid camera movement, layout edits, and editor state updates.

## Files
- `input_handler.h` / `input_handler.c` — central dispatcher invoked from `main.c`. It fetches `GlobalState`, rebuilds hitboxes before and after event handling so selection stays in sync with layout changes, and forwards each event to the mouse and keyboard handlers.
- `input_mouse.h` / `input_mouse.c` — handles mouse wheel zoom, left-click selection (anchors take priority over walls), right-click wall placement via `Editor_ClickAt`, and drag-to-pan behaviour. It also calls `UIPanel_HandleClick` so UI interactions short-circuit layout edits.
- `input_keyboard.h` / `input_keyboard.c` — polls held keys for continuous panning and zoom, toggles the global quit flag on `Esc`, and reacts to discrete key presses: `Shift` constrains wall placement, `D` toggles delete modes, `O` recentres the grid on the selected anchor, `P` toggles anchor persistence, and `Delete/Backspace` remove the selected wall or anchor.

## Interactions
- Mouse and keyboard handlers depend on `Global_Get()` to access the grid, layout, and editor.
- `HitboxSystem_Rebuild` produces clickable bounds in screen space so selection logic can work with the rendered geometry.
- UI button actions triggered from `UIPanel_HandleClick` call into layout persistence, grid zoom helpers, and editor mode toggles.
