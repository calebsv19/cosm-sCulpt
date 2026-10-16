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

`line_drawing_view_controls` owns the camera/visibility header buttons and transient searchable popup. It delegates camera sessions, inspection visibility and appearance to their existing authorities; optional status chips yield space to these controls. Popup rendering runs after pane chrome and before document dialogs.

The visibility popup delegates Coverings membership to `layout_coverings` and
nonphysical group membership to `layout_guide_groups`. Restore covers is scoped;
it preserves the current Guide hidden state and other independent inspection gates.


The Visibility header delegates to the persistent panel in `UI/panel/ui_panel_visibility.c`. `visibility_tree.c` builds separate semantic scene and expression-layer rows; legacy documents retain their existing fallback tree.
