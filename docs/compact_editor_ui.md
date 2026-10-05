# Compact editor controls

The scene editor has five primary right tabs in one row: **View**, **Create**,
**Object**, **Measure** and **Routes**. The selector below View or Object chooses
its working surface; it is not another permanent row of tabs.

| Task | Location |
| --- | --- |
| Camera, appearance, sections | View → Camera / section |
| Saved views and visibility filters | View → Visibility / saved views |
| Dimensions, thickness, transform, gizmo | Object → Geometry |
| Names, types, tags and membership | Object → Details / tags |
| Group movement and members | Object → Assemblies |
| Attachments and containment | Object → Connections |
| Keep-out, service and motion volumes | Object → Volumes |
| Obstruction validation | Object → Checks |
| References, constraints and saved movement | Measure |
| Corridors, routes, inventory and electrical details | Routes |

In the separate Object asset workspace, Tools and Properties retain their existing
labels. Properties offers Geometry and Edit tools, including body/face/edge/vertex
selection. The underlying tools, stable IDs, scene schema and Undo are unchanged.

View puts camera/appearance controls first, followed by Section and Interaction.
Section starts with Off / Slice / Cutaway. Enabling Slice or Cutaway reveals X/Y/Z,
the slider, numerical Position and Step, Previous/Next and focus controls. X cuts
in YZ, Y in XZ, and Z in XY. Cutaway additionally offers retained-side flipping.
Section help is collapsed by default. These controls work with any scene, not
only a vehicle. Sections are temporary viewing state and do not change geometry.

Forms, tab captions and control spacing are smaller. Fonts still follow the
chosen shared font preset and user zoom, with a compact app default. View and
Object flow from the top rather than stretching summary cards or anchoring
controls across large empty gaps. Scrollbars remain available for long inventories,
small windows or enlarged fonts; the change does not force every form into a
fixed height at the cost of hidden controls.

The full van remains the application example. The isolated cabinet is a focused
V1/V2 test fixture. Use visibility filters to inspect construction in the full van,
or select a cabinet panel and use Focus assembly to examine its section. Other
assemblies are still tentative; this UI change does not finish their joinery,
mounting details, fabrication dimensions or blueprint output.
