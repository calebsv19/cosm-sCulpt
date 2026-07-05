# Layout Primitives

This subtree owns primitive creation and resize behavior for authored scene
objects.

## Ownership

- `layout_primitives_create.c` owns plane and rectangular-prism creation
  contracts.
- `layout_primitives_resize.c` owns shared plane/prism resize helpers.
- `layout_primitives_rect_prism_resize.c` owns rectangular-prism depth and
  constrained resize behavior.

## Boundary

- Keep primitive geometry mutation here.
- Keep object-authoring semantic operations in `src/ObjectAuthoring/`.
- Keep viewport drag policy in `src/Input/` and gizmo target rules in
  `src/Editor/gizmo/`.
