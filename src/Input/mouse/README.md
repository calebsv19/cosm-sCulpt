# Input Mouse Drag

This subtree owns extracted mouse-drag session helpers.

## Ownership

- `input_mouse_drag.c` owns shared drag lifecycle helpers for anchors, objects,
  object-handle gizmos, topology targets, and scene-bounds gizmo drags.

## Boundary

- Keep high-level mouse event dispatch in `src/Input/input_mouse.c`.
- Keep hover and pick policy in `src/Input/input_mouse_hover.c` and
  `src/Input/input_viewport_pick.c`.
- Keep primitive/scene mutation APIs in `src/Layout/`.
- This file is over the warning threshold and is a future R5/decomposition
  candidate; new drag feature lanes should prefer a sibling module when the
  boundary is clear.
