# Editor Module

The editor layer manages the wall-placement workflow, bezier anchor editing, multi-anchor selection/dragging, and overlay rendering for in-progress actions.

## Files
- `editor.h` / `editor.c` — define `EditorState`, track the current tool mode, anchor/hover selections, delete mode, shift-state, marquee selection bounds, and multi-selection arrays. Maintain undo/redo stacks (JSON snapshots of the layout), expose `Editor_HistoryCapture`, `Editor_Undo`, `Editor_Redo`, `Editor_ClearHistory`, and implement helpers for selection editing (`Editor_SelectAnchor`, `Editor_SelectAnchorsInBox`, `Editor_BeginAnchorDrag`, etc.). `Editor_ClickAt` still powers two-click wall placement with optional axis locking.
- `render_editor.h` / `render_editor.c` — render helper functions: `Render_Editor_Anchor` shows the starting anchor while placing a wall, `Render_Editor_GhostWall` draws the snapped preview line (respecting shift locking), `Render_Editor_SelectionBox` draws the translucent marquee while lasso selecting, and `Render_EditorOverlay` bundles the overlay calls. A placeholder exists for future selected-wall highlighting.

## Interactions
- `Input_MouseHandle` calls `Editor_ClickAt` on right-clicks with grid-snapped positions, feeds hover information back into `EditorState`, and now drives selection workflows (shift-click, marquee, double-click focus, Alt drag). The editor exposes helper APIs so the input layer can add/remove anchors without duplicating array logic.
- `Input_KeyboardHandle` toggles `shiftHeld`, delete modes, origin shifting, pinning, converts anchors between corner/curve modes, toggles linked bezier handles, deletes selected elements, and wires `Ctrl/Cmd + Z` / `Y` into the undo stack.
- UI buttons (load/save JSON, reset origin, pin anchor, link handles, zoom) call `Editor_HistoryCapture` before mutating the layout so operations remain reversible, and invoke `Editor_ClearHistory` when a new config is loaded or saved as a baseline.
- `Render_Frame` invokes the overlay functions so the editor feedback (ghost walls, marquee, drag colouring) appears between layout drawing and UI rendering. The info overlay pulls selection/handle metadata from `EditorState` to display group counts, drag mode, and bezier values.
