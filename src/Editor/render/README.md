# Editor Render

This subtree owns editor overlay rendering.

## Ownership

- `render_editor.c` renders editor feedback such as starting anchors, ghost
  walls, marquee selection, and free-view axis gizmos for anchors, selected
  objects, scene bounds, and object-authoring topology selections.

## Boundary

- Keep overlay visuals here.
- Keep this subtree read-only with respect to app state: derive draw commands,
  labels, colours, and projected geometry from `GlobalState`, `EditorState`,
  and `Layout`, but do not mutate those owners from render code.
- Keep top-level frame ordering in `src/Render/`.
- Keep geometry drawing in `src/Layout/`.
- Keep UI panel/topbar rendering in `src/UI/`.
