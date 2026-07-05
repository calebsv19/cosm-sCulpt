# UI Topbar

This subtree owns the production top-pane menu/status surface for the editor.

## Ownership

- `line_drawing_editor_topbar.c` renders and hit-tests the coordinated topbar:
  workspace switch, selected-object context, file/dirty state, mode, view,
  plane, construction-plane readout, bounds, gizmo, live operation, and
  undo/redo chips.

## Boundary

- Topbar chips mirror existing backend actions. Do not invent separate state
  ownership here.
- `CP` remains a readout until a construction-plane picker/stepper workflow is
  deliberately designed in its own feature slice.
- Keep broader panel/sidebar controls in `src/UI/panel/`.
