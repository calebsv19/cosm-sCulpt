# Solid panels and movable sections (V1 / V2)

Implemented 2026-10-05. These are viewing and native panel-editing slices; they do
not certify the van dimensions or cabinet construction.

## View controls

Scene → View contains a bordered **Section** group:

- **Off** restores whole-object drawing.
- **Slice** shows the exact intersection of the plane with native solid prisms.
- **Cutaway** clips one half of native solids and closes each cut panel with a cap.
- **X**, **Y** and **Z** select the axis along which the section moves.
  The section header shows the perpendicular plane (YZ, XZ or XY). Vertical orthographic sections show +Z upward.
- The visible slider spans currently shown physical objects. **Position** accepts
  a number with an optional unit suffix; **Step** controls **Previous / Next**.
  Click a field, type, then **Apply**, Return or **Cancel**.
- **Keep + side / Keep - side** switches the retained cutaway half. Cutaway
  initially removes the near half of the orthographic view. Use **Orbit** below
  to inspect the same clipped solids in 3D; Section stays orthographic.
- **Focus assembly** filters to the selected object's parent assembly and
  fits it. **Show all / fit** clears active semantic filters and fits the scene.

The pane scrolls when the controls do not fit. Solid / Material / Wire / Bounds
remain in Camera / appearance above Section. Wire or Bounds turns section clipping off.
Section direction overrides viewing only: it does not change the construction
plane, dimensions, transforms, document dirty state or Undo history.

Section positions and active filters are temporary view state, reset on document
load or workspace switch. Saved slice bookmarks, finite-thickness slabs, hatching,
dimensions on sections and blueprint export are later work.

## Solid / Material and selection

Scene **Solid** and **Material** render native planes and rectangular prisms into
the same software depth/owner buffer as runtime mesh instances. Nearer physical
surfaces hide farther surfaces. Material uses simple colors for known wood,
steel/aluminum and foam text properties; absent/unknown material stays neutral.
These are appearances, not a new density/strength/material database.

Solid selection follows the visible surface, including section caps. Empty section
pixels cannot select an object's hidden center or a clipped-away gizmo. Selected
and hovered objects retain explicit feedback. Reserved/service/motion/corridor
volumes remain explanatory wire overlays in ordinary viewing, not opaque materials;
section views hide those overlays, routes, lights, paths and manipulators.

Native closed panels have outward face winding and normal-based flat shading.
Depth visibility currently replaces an explicit user-selectable hemisphere/culling
mode. This is not photorealistic rendering or a general solid-modeling kernel.

## Thickness editing

Select a native prism explicitly typed **Panel**, then open Scene → Object.
Below dimensions, **Thickness**, **Keep** and **Material** are bordered controls.
Thickness is the smallest local extent. The Keep control cycles center, negative
and positive local thickness face (U = width, V = height, N = depth). Choosing a
face preserves that face's world position when thickness changes, including a
rotated panel. Enter `12.7 mm`, `18 mm` or another positive length in the existing
dimension dialog. Apply participates in geometry validation and one-action Undo.

Thickness must remain smaller than both panel spans. Locked/scaled panels and
constraint/bounds conflicts reject the edit. Existing W/H/D edits remain available.
This slice does not automatically resize neighboring panels or prove joinery.
Material cycles tentative plywood, steel, foam and unknown using existing metadata
and Undo; it does not select construction-grade product specifications.

Managed construction panels use **Edit unit** instead of individual geometry
changes: see [Furniture units](furniture_units.md). Their thickness updates the
whole shell in one transaction; individual edits that conflict with its recipe
are rejected. Unmanaged panels retain the controls above.

## Existing cabinet acceptance example

`config/examples/cabinet_view_v1.layout.json` is an isolated study extracted from
`van_connected_sections_s5.layout.json`. Its seven panel geometries and stable IDs
are unchanged: backing, front/rear ends, bottom, top, door and shelf. Internal
support/attachment links are retained. Other van objects, external links, routing
and bed motion are excluded from this study. Missing panel material is labeled
`plywood_tentative` for appearance. The full original van fixture is unchanged.

1. Open **cabinet_view_v1.layout.json**, choose View → Material, then Fit scene.
2. Choose Slice, then Y. The middle slice shows two vertical panel edges,
   top, bottom and shelf, with empty space between them.
3. Drag the slider or use Position, Previous and Next. At an end panel the section
   becomes filled, representing that panel's real solid thickness.
4. Choose Cutaway, then Orbit or flip the retained side. Off restores the cabinet.
5. Select a panel, open Object, change Thickness, then Undo. The view remains
   derived from the same document geometry.

The existing example panels are approximately 18 mm thick; this is still concept
geometry, not a measured plywood specification or a finished construction plan.

## Evidence and boundaries

Six Sections tests cover outward faces, exact and boundary intersections,
cutaway cap winding, physical unit conversion, rotated anchored thickness,
failed-edit atomicity, save/reload, Undo/Redo, a visible thickness form, depth
order independence and actual cabinet pixels. Section controls include drag,
numeric input and stepping; view-only operations preserve serialized geometry.
The native Vulkan capture harness additionally verifies shelf/void surface picking
and ignores an injected stale gizmo hitbox.

Reproduce native captures with `LINE_DRAWING_VISUAL_ARTIFACT=/tmp/cabinet.bmp` and
`LINE_DRAWING_VISUAL_ARTIFACT_MODE=cabinet-solid`, `cabinet-section` or
`cabinet-cutaway` on the built app. These modes stage the full original fixture,
focus its cabinet and exit after capture; they do not write working scene files.

Exact sections support native rectangular prisms. Zero-thickness planes and
imported meshes are omitted from exact sections. Imported mesh cutaways are clipped
but uncapped. Cut geometry is never saved or exported as authoritative geometry.
