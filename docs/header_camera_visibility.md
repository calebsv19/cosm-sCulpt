# Header camera and visibility controls

Free Camera activates the same movable observer as Explore in the View sidebar.
Its ON state distinguishes movement from the editor's Free orientation. Click it
again to return to the editor. More offers Enter selected saved camera, Return to
editor, Return to plane editor, editor orientation, 2D/3D, bounds, Gizmo and
Wire/Solid/Material appearance. Saved camera selection remains in the View sidebar.

Visibility opens temporary object and assembly inspection controls. [x] means all
live objects in the subtree are shown, [ ] means hidden, and [-] means mixed.
Click a hidden row to Show it, or a shown/mixed row to Hide it. Show reveals saved
hidden geometry and bypasses temporary category and saved-view filters. A group
action replaces earlier overrides in its subtree; a later child action takes
precedence over its parent. Deleted objects remain excluded and native selectable
flags still govern picking. No saved visibility flags or geometry are changed.

Only reveals the chosen subtree and hides everything outside it. The isolation
boundary takes precedence over Show outside that subtree; choose Only on another
row or Restore to leave it. Restore clears inspection overrides and returns to
the preset's saved visibility and the prior category/saved-view filters. Inspection
state remains outside save bytes and document history. Sidebar Hide/Show/Only and
Restore share this model.

Search matches labels, types, stable IDs, properties and ancestor assembly labels;
wheel or arrow keys scroll results. Hidden rows stay in the list. Camera, Visibility
and More retain header space as the window narrows. Gizmo sits immediately left of
Visibility when space permits; More retains Gizmo in compact layouts. Escape or an
outside click closes the popup. Opening it ends active camera gestures; search and
popup scrolling cannot move the scene.

## Appearance and grouped areas

The header appearance selector shows Wireframe, Solid or Material and changes
the surface immediately at the current pose. Selecting Solid or Material from
2D enters 3D; switching back to 2D uses its line editor. Inspection keeps the
chosen surface mode. Material and Solid use clean surfaces by default; optional
subdued edges are independent of appearance. Selection and hover remain emphasized.

Visibility opens a collapsed assembly tree from the scene's existing parent
relationships and labels. Click the arrow/name to expand or collapse; the separate
eye toggles actual effective visibility. Search expands matching branches and
shows ancestors. Existing roof/equipment, bed/kitchen/cabinets, electrical/chase
and reference areas use authored grouping; ambiguous ownership is not inferred.

Moving previews retain half-logical resolution and adaptive triangle selection.
Settled previews use drawable DPI up to1.25logical scale, capped below900k pixels,
with a local edge coverage filter. The filter preserves depth and pick owners.
Triangle120k and prepared asset budgets are unchanged. This improves raster edges;
underlying geometric facet boundaries are still the authored/LOD mesh.

Unparented objects use existing subsystem, explicit guide or reference metadata for
presentation folders. Objects without that information stay in Ungrouped. These
folders do not change native ownership. Virtual area actions retain the inspection
model's1024-entry override limit; an over-capacity action is refused with a visible
message so a smaller branch can be chosen. Native assembly subtrees use one override.

### Functional guide groups

`Guides / nonphysical space` collects spatial guides across every native assembly,
including assembly-owned members. Expand it for Motion zones, Walkways, Keep-clear /
keepouts, Service / access space, Routing corridors, Drawing annotations / bounds and Other spatial guides; only
categories present in the document appear. Classification uses native types, volume
roles, motion records and explicit `view_group` values: `motion`, `walkways`, `keepouts`, `service`,
`corridors`, `guides`, and `annotations`. A subsystem
label cannot move a guide into a physical group. Lack of a surface material alone
never makes a physical object a guide. Authoring assembly relationships are unchanged.

The guide parent eye and View-panel Guides button control the same complete spatial
collection and camera/path authoring helpers. Hide removes all these members; Show
can temporarily reveal authored-hidden members. Child eyes and Only target their
functional members. Restore clears temporary object overrides and returns camera/path
helpers to their state before the guide-parent action. Guide collection operations
retain the existing temporary override limit and refuse an oversized group.

Guides off also suppresses the scene-bounds outline without changing its saved
enabled flag. Restore returns helper visibility to its earlier state. Older untyped
fan opening markers, cable reservation lines, panel cell/T-slot schematics and
explicitly labeled native motion carriers join their corresponding functional groups.
Generic hardware/device envelope labels stay physical.

Intentional Wireframe appearance, selection/hover outlines and
physical reference parts have their own controls; they are not movement/clearance
volumes. Their outlines do not imply a missing physical material.

Functional guide children stay together in a stable order. The Assemblies parent
eye controls its physical members and preserves guide overrides, so showing that
collection cannot bring back movement zones or walkway space. Native ownership
and individual assembly controls remain available.

Saved guide presets use the same functional membership as the visibility tree.
Motion, Walkways, Keepouts, Service, and Corridors include corresponding legacy
helpers and explicit `view_group` metadata. A hidden functional preset excludes
its helpers even when they also match OEM, Furniture, or a temporary reveal.
Physical envelopes such as rails, solar panels, and hinge hardware remain physical.
New default lists include aggregate Guides and Annotations presets; existing
lists retain their names, IDs, and document bytes. Guide selectors use
`guide_category` with `all`, `motion`, `walkways`, `keepouts`, `service`,
`corridors`, `guides`, or `annotations`.
Showing the Guides parent also enables hidden functional guide presets. Restore
returns those presets and the editor helpers to their pre-parent-action state.

## Coverings and scoped restore

The visibility tree also exposes Coverings → Doors / Drawers / Access panels →
area → item. Membership comes from native `cover_group` and `cover_part`
metadata. The parent toggle retains individually hidden children, and mixed
states remain visible. **Restore covers** clears only covering inspection state;
Guides off continues to hide movement zones, walkways, keepouts, service space,
routing reservations and annotations. Saved covering and guide presets use the
same native classifiers as the tree and agent inspection route.


Main Edit development now uses a persistent Scene/Layers/Views panel with drawn eye/disclosure controls. See [scene collections](scene_collections.md) for functional grouping and named-view behavior.
