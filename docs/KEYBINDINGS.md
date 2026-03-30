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

## Navigation
- `Arrow Keys`: Pan the current view.
- `=`: Zoom in.
- `-`: Zoom out.
- `Mouse Wheel`: Zoom in/out.
- `Esc`: Quit app.

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
- `Shift + Left Drag` on empty space: Marquee selection box.
- `Shift + Left Click`: Add/remove from anchor selection group.
- `Double Left Click` on selected anchor: Collapse multi-selection to that anchor.
- `Left Drag` on empty space: Pan view.

## UI Buttons (Mouse)
- `Save JSON`: Save current layout config.
- `Load JSON`: Load a layout config.
- `Export Shape`: Export shape asset.
- `Reset Origin (O)`: Same action as `O` key.
- `Zoom In (+)`: Same action as `=`.
- `Zoom Out (-)`: Same action as `-`.
- `Toggle Delete (D)`: Same action as `D`.
- `Pin Anchor (P)`: Same action as `P`.
- `Link Handles (L)`: Same action as `L`.
- `Mode: <2D|3D> (M)`: Same action as `M`.

## Notes
- Selection prefers: handles, then points, then walls.
- Overlapping candidates are resolved by ranked picking using type priority, active-plane depth distance, and cursor proximity.
