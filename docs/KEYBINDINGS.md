# LineDrawing Keybindings And Input Map

This file is the single reference for keyboard and mouse interactions in the editor.

## View And Camera
- `M`: Toggle space mode between `2D` and `3D`.
- `V`: Toggle view mode between `PLANE_VIEW` and `FREE_VIEW`.
- `1`: Set active edit plane to `XY`.
- `2`: Set active edit plane to `YZ`.
- `3`: Set active edit plane to `XZ`.
- `[` / `]`: Move active plane offset by one grid unit.
- `Shift + [` / `Shift + ]`: Move active plane offset by 10 grid units.

### Mode Rules
- In `2D` mode:
  - active plane is locked to `XY (z=0)`,
  - free-view camera controls are disabled,
  - `V`, `1/2/3`, and `[`/`]` plane controls are blocked.
- In `3D` mode:
  - free-view and plane controls are enabled.

### Free View Mode Controls
These controls apply when view mode is `FREE_VIEW`.
- `Q` / `E`: Yaw left / right.
- `T` / `G`: Pitch up / down.
- `J` / `L`: Move view target left / right.
- `I` / `K`: Move view target up / down.
- `Alt/Option + Mouse Move`: Orbit camera around the current layout centroid (no mouse click required).
- In `SPACE_MODE_3D + FREE_VIEW`, selecting an anchor shows six translate gizmo handles (`+/-X`, `+/-Y`, `+/-Z`).

## Navigation
- `Arrow Keys`: Pan the current view.
- `=`: Zoom in.
- `-`: Zoom out.
- `Mouse Wheel`: Zoom in/out.
- `Shift + C`: Toggle center crosshair overlay.
- `Esc`: Quit app.

## Data Roots
- `Ctrl/Cmd + B`: Open native folder chooser and set input root.
- `Ctrl/Cmd + Shift + B`: Open native folder chooser and set output root.
- `Ctrl/Cmd + I`: Open typed input-root edit dialog.
- `Ctrl/Cmd + O`: Open typed output-root edit dialog.

## History
- `Ctrl+Z` / `Cmd+Z`: Undo.
- `Ctrl+Shift+Z` / `Cmd+Shift+Z`: Redo.
- `Ctrl+Y`: Redo.

## Editing
- `Shift` (hold): Axis-lock wall placement while placing a segment.
- `D`: Toggle delete mode (`SAFE` / `AUTO_PRUNE`).
- `O`: Recenter origin to selected anchor.
- `P`: Toggle selected anchor persistent/pinned state.
- `C`: Toggle selected anchor between corner/curve (requires valid topology).
- `L`: Toggle selected curve anchor handle linking (when not consumed by free-view control mode).
- `Delete` / `Backspace`: Delete selected wall/anchor.

## Mouse Interaction
- `Right Click`: Place wall points (two-click workflow), constrained to active edit plane.
- `Left Click`: Select handle/anchor/wall (ranked selection).
- `Left Drag` on selected anchor(s): Move selected anchor group (plane-constrained).
- `Alt + Left Drag`: Precise drag (no snap).
- `Left Click` on gizmo handle (`3D + FREE_VIEW`): Start axis-constrained gizmo drag for the selected anchor.
- `Left Drag` on gizmo handle (`3D + FREE_VIEW`): Move along selected axis with signed motion (pull opposite to move negative).
- `Shift + Gizmo Drag` (`3D + FREE_VIEW`): Smooth/non-quantized gizmo drag.
- `Shift + Left Drag` on empty space: Marquee selection box.
- `Shift + Left Click`: Add/remove from anchor selection group.
- `Double Left Click` on selected anchor: Collapse multi-selection to that anchor.
- `Left Drag` on empty space: Pan view.

## UI Buttons (Mouse)
- `Save JSON`: Save current layout config.
- `Load JSON`: Load a layout config.
- `Export Shape`: Export shape asset.
- `Input Edit`: Open typed input-root edit dialog.
- `Input Folder`: Open native folder chooser for input root.
- `Output Edit`: Open typed output-root edit dialog.
- `Output Folder`: Open native folder chooser for output root.
- `Reset Origin (O)`: Same action as `O` key.
- `Zoom In (+)`: Same action as `=`.
- `Zoom Out (-)`: Same action as `-`.
- `Toggle Delete (D)`: Same action as `D`.
- `Pin Anchor (P)`: Same action as `P`.
- `Link Handles (L)`: Same action as `L`.
- `Mode: <2D|3D> (M)`: Same action as `M`.

## Notes
- Selection prefers: handles, then points, then walls.
- In `3D + FREE_VIEW`, gizmo handles are highest-ranked picks while hovered.
- Overlapping candidates are resolved by ranked picking using type priority, active-plane depth distance, and cursor proximity.
