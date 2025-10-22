# Editor Module

The editor layer manages the wall-placement workflow and provides overlay rendering for in-progress actions.

## Files
- `editor.h` / `editor.c` — define `EditorState`, track the current tool mode, anchor selections, delete mode, and shift-state. Implements `Editor_ClickAt` (two-click wall placement with optional axis locking) and `Editor_SetShiftHeld`.
- `render_editor.h` / `render_editor.c` — render helper functions: `Render_Editor_Anchor` shows the starting anchor while placing a wall, `Render_Editor_GhostWall` draws the snapped preview line (respecting shift locking), and `Render_EditorOverlay` bundles the overlay calls. A placeholder exists for future selected-wall highlighting.

## Interactions
- `Input_MouseHandle` calls `Editor_ClickAt` on right-clicks with grid-snapped positions.
- `Input_KeyboardHandle` toggles `shiftHeld`, delete modes, origin shifting, pinning, and deletes selected elements.
- `Render_Frame` invokes the overlay functions so the editor feedback appears between layout drawing and UI rendering.
