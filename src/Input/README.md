# Input Module

The input layer translates SDL events into grid camera movement, layout edits, selection workflows (click, marquee, and multi-anchor drag), and editor state updates.

## Files
- `input_handler.h` / `input_handler.c` — central dispatcher invoked from `main.c`. It asks `Global_RebuildHitboxesIfDirty()` for fresh hitboxes before forwarding events to the device-specific handlers and routes `SDL_TEXTINPUT` / mouse motion into the UI layer so the save-dialog text box and load dropdown can react.
- `input_mouse.h` / `input_mouse.c` — handles mouse wheel zoom, lasso selection, shift-click additive selection, double-click collapse of selection groups, left-drag group movement (with Alt overriding snapping for the primary anchor), right-click wall placement via `Editor_ClickAt`, drag-to-pan behaviour, and free-view orbit control via `Alt/Option + mouse move` (no mouse button required). In `SPACE_MODE_3D + FREE_VIEW`, clicking a gizmo axis handle starts an axis-constrained drag session with signed motion from drag-start, default grid-step quantization, and `Shift` smooth override; undo is captured once at drag start and the session closes on mouse-up. Wheel zoom now consumes high-precision scroll deltas and zooms around the cursor for smoother trackpad/mouse behavior. It also calls `UIPanel_HandleClick` so UI interactions short-circuit layout edits, updates hover highlights, and raises `Global_FlagGridChanged` whenever panning/zooming alters the camera.
- `input_keyboard.h` / `input_keyboard.c` — polls held keys for continuous panning and zoom, toggles the global quit flag on `Esc`, and reacts to discrete key presses: `M` toggles `SpaceMode` (`2D`/`3D`) with persistence, `Shift` constrains wall placement (and signals shift state to the editor for marquee activation), `D` toggles delete modes, `1/2/3` switch active edit planes (`XY/YZ/XZ`), `[`/`]` adjust active plane offset, `O` recentres the grid on the selected anchor, `P` toggles anchor persistence, `Shift+C` toggles center crosshair visibility, `C` converts anchors between corner/curve, `L` toggles linked handles, and `Delete/Backspace` remove the selected wall or anchor. `Ctrl/Cmd+Z` performs undo, `Ctrl/Cmd+Shift+Z` or `Ctrl/Cmd+Y` performs redo, and the handler defers to the panel when the save-dialog is capturing keyboard input.

## Interactions
- Mouse and keyboard handlers depend on `Global_Get()` to access the grid, layout, and editor. The mouse path now mirrors editor functionality: shift starts marquee selection, Alt overrides snapping during drags, and multi-selection data is stored in `EditorState`.
- During the 3D migration phase, mouse picks are now resolved against the active edit plane using shared ray/plane helpers, then flow through 3D-ready editor/layout APIs.
- `SpaceMode` constraints are enforced in input:
  - `2D` mode locks editing to `XY@z=0` and blocks free-view/plane-switch controls,
  - `3D` mode enables free-view and plane controls.
- Input projection/mapping now flows through `Core/space_mode_adapter` so mode-aware plane/camera behavior is shared with render and hit-test code.
- Curved-handle dragging now derives polar handle values from world-space deltas projected onto the active edit plane, so handle edits remain plane-local and camera-independent.
- In `FREE_VIEW`, Alt/Option orbit updates free-camera yaw/pitch and keeps the orbit target at the layout centroid for fast model inspection.
- While a gizmo drag session is active, orbit is suppressed so axis-constrained editing is not interrupted.
- Overlap resolution now relies on ranked hitbox picking (handles/points over walls, then near-plane and near-cursor tie-breaking), improving selection stability in multi-depth scenes.
- `Global_RebuildHitboxesIfDirty` centralises hitbox regeneration so the input layer never runs stale selection or handle hit-tests.
- UI button actions triggered from `UIPanel_HandleClick` call into layout persistence, grid zoom helpers, editor mode toggles, and history capture (raising the relevant dirty flags as needed). The save dialog / load dropdown continue to consume input so accidental layout edits are avoided while typing or choosing configs.
