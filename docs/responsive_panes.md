# Responsive editor panes

Implemented 2026-10-05. This pass repairs editor pane reflow while preserving
[demand rendering](demand_rendering.md).

## Behavior

Drag either side divider to give the scene list or tool controls more room.
Controls, summaries, tab headers, wrapped help and scroll limits use the current
solved pane bounds. Their hit targets use the same layout as drawing. Widening a
pane exposes longer labels without increasing the text size; narrowing it
reflows the existing rows. Compact action widths and padding remain based on
font metrics. Very narrow panes can still clip long labels; widening the pane
provides additional label space.

In **View → Visibility**, each saved view has a flexible name/toggle area and a
compact **Only** action. **Manage views** expands a separate list of explicit
**Remove view: name** actions. Removing a query is undoable and does not delete
scene objects. **Custom query / save view** remains a separate disclosure.

## Layout invalidation contract

`LineDrawingPaneHost_UpdateSplitterDrag` solves pane bounds and returns whether
they actually changed. The input adapter snapshots the old viewport, updates
the splitter, and calls `Global_RefreshPaneLayout` only on change. Child layout
and picking are refreshed before hover/click processing. The existing viewport
resize bridge retains the effective camera framing relative to the canvas
center. Resizing panes does not edit geometry, dirty the saved document, or
create an undo entry.

Actual window resize uses the same refresh after solving the host. Font-size
shortcuts and workspace font/pane draft changes explicitly refresh child
controls even when the window dimensions remain unchanged. The unchanged-size
window guard remains in place: settled frames do not poll/rebuild UI layout.
The existing shared pane splitter, layout solver and viewport bridge are reused;
no shared API or version change is introduced.

## Regression coverage

`make test ARGS=UIPanelResize` exercises native mouse dispatch through both
splitters, repeated unchanged motion, left Scene/File bounds and scene-list
selection in the newly exposed area, every right tab, measurement/route control
bounds, header overlap, projected camera framing, geometry/history preservation,
visibility toggling/isolation, separate management with remove/undo, font metric
changes and actual window resizing. The full host suite remains required for
input, preview, scroll, undo and demand-loop compatibility.

For desktop acceptance, use the saved van scene, widen and narrow both panes,
check View/Create/Object/Measure/Routes and Scene/File, then stop interacting and
measure idle CPU. This does not establish a frame-rate or input-latency benchmark.
