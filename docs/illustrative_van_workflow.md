# Illustrative van workflow

This is a teaching and regression project, not a measured vehicle or an approved
build. The placeholder interior is 1850 mm wide, 3650 mm long and 1900 mm high.
Verify actual boundaries, ribs, openings, mounting points and wheel wells before
using it for purchasing, fabrication or physical installation.

The native document uses meters and right-handed Z-up. The example assigns +X
passenger, +Y forward and +Z upward, with origin at the floor center. Display units
are independent. Object IDs survive naming, movement and assembly reparenting.

## Starting document and safe regeneration

The tracked starting point is
[illustrative_van.layout.json](../config/examples/illustrative_van.layout.json).
Keep it pristine; save a separate working copy outside the checkout. This audit
created `van_starting_example.layout.json` and editable `van_working.layout.json`
in `~/Library/Application Support/LineDrawing-Main-Edit/projects/van_workflow/`.
This project directory is outside the replaceable package/runtime payload. The
original Desktop workspace copies are preserved, but the open editor uses this
Application Support working copy. The generator refuses an existing destination;
it never refreshes or replaces a user's working file.

```sh
make agent_scene_tool
build/toolchains/clang/bin/agent_scene_tool --example-van /tmp/new-van.layout.json
make illustrative-van-smoke
```

`--example-van` is a separate, create-only native document generator. It composes
existing Layout APIs, serializes and reloads, and audits both the intentional
obstructions and a repaired candidate. Its stdout is an audit JSON receipt. It
cannot be combined with request generation or read-only checking. The generic
`line_drawing_agent_scene_request_v1` fields have not been expanded to accept
assemblies, constraints or envelopes.

There are 38 objects, eight assemblies, seven relationships, four maintained
geometric constraints, three saved spatial checks and two motion envelopes.
Reference objects and authored assembly members carry `dimensions_status =
illustrative`; generated envelopes retain their source-rule provenance.

## Navigation and legacy lines

The viewport starts in **Select**. Ordinary selection and secondary clicks do not
create points or lines. Existing line/anchor documents remain supported.

- **View → Orbit**: drag with the left mouse button to rotate in 3D. It enters
  Free view. **Option + left drag** also orbits, including when starting over
  a point, handle or object; engineering zoom uses the same physical ceiling as Fit.
- **View → Pan**: left drag moves the view. Right/middle drag pans while selecting.
  Scroll zooms and **Fit scene** frames visible geometry without changing history.
- **View → Select**: return to object/point selection and editing.
- **Create → Geometry → Draw line**: click two endpoints on the construction plane.
  The first endpoint is a small preview, not a saved anchor. The second creates
  one line with endpoints as a single undo step. **Stop drawing**, right click,
  Escape or leaving Create cancels unfinished placement. Choosing a different
  geometry creation tool also cancels it. Draw line stays active after completion
  until stopped; it does not force a connection to the next line.
- Select an existing point or line, then use the left **Delete selected** button
  (or Delete). Deleting a point also removes its incident lines. Deleting just a
  line follows the existing Safe/Auto prune policy. **Undo** restores the deletion.

Camera intent is captured before geometry picking and retained until release.
Orbit/Pan, cancellation and an unfinished endpoint do not change saved geometry.
Dialogs and active authoring/geometry drags retain their existing input ownership.
The toolbar uses existing pane/theme controls, and camera updates reuse the
existing shared viewport bridge. Document schema, entity IDs and unit rules are unchanged.

## Mouse-driven walkthrough

1. Open the working layout using **File → Load Layout**. Select **Measure → Units
   → mm**. Select a named view, then **View → Fit scene**. Fit changes the camera
   and zoom only; it uses visible geometry and does not add an undo entry. Display
   units and view selection currently reset on relaunch; select mm again if needed.
2. In **Measure → Saved constraints**, choose `bed_lift`. It selects Travel,
   Z direction, limits **900–1600 mm** and **Moves: Lift bed**. Drag the slider,
   or use Min/Max/Reset. The deck, mattress and two rails move together. Use a
   side or free view to see the height change; Top hides Z movement. One slider
   gesture is one undo step. Increasing Z now displays upward in side/front/free
   views. Undo clears the active measurement form; reselect
   the saved rule. Save after restoring the starting position if Dirty remains.
3. Choose `water_door_hinge`. The saved **0–90°** motion moves the nested Cabinet
   door assembly, including its handle, around the hinge reference. Reset returns
   it to the saved home. **View envelope** navigates to the derived overview box.
4. Inspect `controller_height`: the controller origin stays **570 mm above the
   pump origin along Z**. This is distinct from the saved **100 mm minimum surface
   clearance** check. Inspect `bracket_right_angle` for coincident offset pivots
   and a maintained **90°** planar angle.
5. In **Object → Assemblies**, inspect the root, OEM reference, Lift bed, Water
   cabinet and its nested Cabinet door. In **Object → Details**, inspect names,
   types, IDs, membership and properties. In **Object → Connections**, inspect the bed
   support links, cabinet/pump attachments and controller/tank/battery containment.
   Containment and support links describe intent; they do not prove fit or strength.
6. Use **View → Visibility** to inspect a subsystem or Reference/Design subset.
   Clear the filter before the overall audit. Hidden objects remain in spatial
   validation. The controller's `network=can0` and power-domain fields are labels;
   there is no routed cable or verified electrical topology yet.
7. In **Object → Volumes**, inspect the walkway, fuse-box service space and the
   two labeled motion overview boxes. In **Object → Checks**, run checks, select
   a motion warning, then **Inspect motion → Check full range**. Preview Previous,
   Next and Close are read-only; they do not move the authored assembly.

## Reproducible obstruction exercises

The initial design intentionally includes two objects with **TEST** names:

| Exercise | Starting result | Example repair (world coordinates in mm) |
| --- | --- | --- |
| `bed_obstruction`: test cabinet | Full-range inspection finds bed contact at a tested 1600 mm position | Keep X = -620 and Z = 1350; set Y = 430 |
| `door_obstruction`: test post | Full-range inspection finds door contact at a tested 45° position; post also intersects walkway | Set X = 800, Y = 1600, Z = 450 |

These are checked poses, not claims of earliest contact. Use the existing numeric
Object position controls for these changes, then rerun checks. The generator
verifies both repaired movement ranges as separated and all three saved spatial
checks as passing, on a reloaded candidate. The saved starting file still contains
the obstructions. Undo the exercises or reload the pristine copy to start again.

The initial spatial report has one walkway error, four motion warnings and one
clearance pass (approximately 477.1 mm vs 100 mm required). There are **two unique
motion obstruction pairs**: automatic and saved pair checks currently repeat each
warning. Conservative envelope overlap is a warning; tested primitive contact is
stronger evidence. Bounds-only evidence is not an exact collision certificate.
Spatial warnings do not prevent allowed constrained motion. A separated report
only covers the modeled geometry, owners, exemptions and saved rules.

## Audit evidence and remaining gaps

The native generator checks persistence, stable IDs, placeholder dimensions,
initial contact findings and repaired range separation. The smoke test also
checks deterministic regeneration against the tracked starting file, read-only
checking, conflicting operation refusal and preservation of an existing edited
file. Viewport mouse regression checks scene fit, physical zoom, visibility,
free-camera centering and unchanged document/history. The host suite passes.
The installed Main Edit was exercised with mouse controls: Fit in top/free views,
mm display, bed slider and undo, saved assembly scope, support links, water filtering
(10 of 38 shown) and Show all, spatial findings/full-range read-only bed preview,
and door Max at 90 degrees followed by undo/save. The restored working copy is
Saved. The host suite passes 491 tests in 47 reported suites; example and existing
producer smoke checks pass. These checks do not establish user acceptance.

The initial real workflow exposed an overly restrictive legacy zoom cap: with a
100 mm grid, the van could remain tiny. **Fit scene** and a physical zoom ceiling
now make meter-based geometry navigable. Scene fit is explicit, not automatic.
The follow-up native input audit verified visible Orbit/Pan drags, first-endpoint
cancellation and right-click without adding anchors, walls or undo entries.
Regression tests additionally cover Option-drag over a selected point and at
physical zoom, explicit drawing/cancel/delete/undo, and unchanged serialized state.
Point pick targets and the pending marker are pixel-sized. Hidden zero-sized
controls no longer repaint section backgrounds over live Create controls.

The viewport follow-up repaired Z-up projection together with inverse picking,
resize mapping and camera navigation. Gizmo axes now track viewport size within
48–100 logical pixels; endpoints and pick targets remain independent of zoom and
object dimensions. Axis foreshortening remains orientation-dependent. Native review
confirmed edge selection, wall gizmos at two zooms and upward bed lift with undo.
The saved working file retains the original digest.

The remaining usability gaps are:

- **Readability follow-up:** the 2026-10-04 refinement below corrects Parts assembly
  chooser wording and groups repeated automatic/saved warnings. Measure object selectors use full-width
  wrapped labels. Motion-envelope labels occupy separate clipped legend rows.
- **Selection limits:** visible wireframe edges and centers are selectable; blank
  projected box interiors are not. Imported mesh edge selection is a bounds proxy.
- **Session continuity follow-up:** the 2026-10-04 refinement below retains the active
  Measure rule through undo and saves units/view/tool preferences on normal exit.
- **File access limits:** the pre-S5 repair below defers remembered-file restore
  until after a host frame and requests directory scans explicitly. Reads remain
  synchronous; a slow filesystem can still delay an explicit request. OS privacy
  grants across changing ad-hoc identities are not guaranteed. The working project
  remains in the app's own Application Support area.

The teaching model does not validate materials, load capacity, manufacturing
clearances, whole-van occupancy, electrical compatibility or real measurements.
Independent moving followers, imported moving meshes and simultaneous joints
remain outside the bounded motion contract.

## Next implementation boundary: S5

Retain this document as a repeatable acceptance project. Projection/picking,
readability, saved-motion discovery and the named pre-S5 continuity/file-access
refinements now have proof recorded below. Implement S5 as small UI-first slices;
see [the detailed routing and measured van plan](s5_routing_plan.md):

1. **Route contract and editing:** stable route IDs, source/destination entity
   references, ordered physical points, numeric coordinates, visible picking,
   add/remove/reorder controls, total length and one undoable edit. Routes are
   authoritative polylines; render tubes are derived. Save/reload/export parity
   and refusal of dangling endpoints belong in the first slice.
2. **Named corridors:** create an illustrative upper low-voltage halo and a
   passenger riser; route the water controller to CAN hub through them. Corridor
   membership expresses routing intent until geometric validation is implemented.
3. **Routing checks:** segment intersections against reserved/service/motion
   space, required clearance and route-length limits, with selectable findings.
   Never claim bend-radius, resistance or voltage-drop validation from a plain
   polyline length alone. Add material/gauge/power contracts before those values.

Van dimensions and layout requirements can replace placeholders incrementally.
Preserve IDs and undo history, regenerate changed envelopes, and rerun the same
walkthrough after each measured revision. Compiler metadata and runtime telemetry
remain later consumers of the authored model.

## Saved-motion discovery refinement (2026-10-04)

Select Bed deck (or a bed-assembly member), then open Measure or click Object /
Move saved motion. `bed_lift` should open directly with its saved range and enabled
slider/Min/Max/Reset. Selecting Cabinet door or an inherited member should open
`cabinet_door_hinge`; its range is shown in degrees. Parts provides the same entry
for the corresponding assembly. No new movement needs to be created.

Edit limits and references expands authoring controls explicitly. Undo/Redo keeps
the rule selected by stable ID and refreshes current values. On normal exit, units,
named/free view, orientation and navigation tool are saved as presentation preferences.
The Parts assembly chooser now says Choose assembly, and repeated check pairs expand
into individual rule results without losing conservative-versus-contact evidence.

Proof for this checkpoint: warning-clean host build, 501 tests across 47 reported
suites, agent-scene-smoke, illustrative-van-smoke, and independently rendered
`parts-motion-travel-group-saved` / `parts-motion-hinge-group-saved` fixtures. The
fixtures visibly show inherited motion controls and linear/angular range guides.
The user's saved working van file remains byte-for-byte unchanged from the previous
handoff (SHA256 `1e58223a8a3e3248586c8cffce0bc5018c623961275d75fcefc9e299654f40e3`).

Desktop handoff was completed after explicit user approval. Native installed-app
testing selected Bed deck #12 and opened bed_lift directly. Max moved the deck
from 900 to 1600 mm, the mattress from 990 to 1690 mm and the driver rail from
860 to 1560 mm. Selecting Mattress #13 discovered the inherited bed_lift; a slider
gesture moved the complete assembly to about 1248.46 mm. A single Undo restored
900/990/860 mm and kept the compact saved-motion panel open. Reset also restored
the 900 mm home pose. No new rule was created and no test pose was saved.

Normal quit was confirmed with a stopped-process audit before relaunch. Resume
Editor then showed the Saved working van with 38 objects, mm units and Free 3D
view. The file digest above is unchanged. Zoom/pan, selection and the active tab
are not persisted: use View / Fit scene, select Bed deck #12, then Measure after
relaunch. The handoff leaves this ready for testing. Undo currently clears selection
and retains Dirty even after returning to the saved pose; the motion controls remain
usable. These are follow-up continuity refinements, not a claim of full human
acceptance. Audit startup framing/browser I/O before S5 route editing.


## Pre-S5 continuity and file-access closeout (2026-10-04)

Undo and Redo retain Bed deck #12 through stable identity. Max moves its complete
assembly to 1600 mm; Undo returns the deck/mattress/rail to 900/990/860 mm and shows
Saved. Redo shows Dirty at 1600 mm with the same object selected. A slider gesture
to about 1248.46 mm also returns to Saved 900 mm with one Undo and the compact
Measure panel retained. Opening/relaunching the working file now fits visible
geometry automatically. Individual zoom/pan, selection and active tab are still
not restored as a session.

The host menu first shows unread sections as an ellipsis. Click Layouts/Scenes/
Recents/Browse to discover files; choose the same section again to refresh. Browse
provides Choose input folder and Choose output folder. Missing/inaccessible roots
show a visible error with retry guidance. Failed file opens preserve the document
and Undo history. The macOS picker is now an app-owned Cocoa panel rather than an
external script that blocks the editor's event/accessibility loop. Native Cancel
preserved the root; Open selected the existing van_workflow folder successfully.
No broad privacy settings were changed and no new protected-folder grant is
claimed. Requested scanning and remembered-file loading are still synchronous.

Warning-clean build, 504 host tests / 47 reported suites, 4 folder-picker contract
tests and both scene smoke lanes pass. Native movement/history, load framing and
folder chooser behavior were inspected on Desktop. The user's working van digest
above is unchanged; no test pose was saved. These results supersede the specific
Undo/highlight/Dirty/framing quirks recorded in the earlier handoff.

S5A manual route editing is the next coding boundary. A measured-shell/reference
pass can proceed alongside it when blueprint renders and verified dimensions are
provided. Remaining CAD/general-motion limitations are not silently closed by this
UI refinement.
