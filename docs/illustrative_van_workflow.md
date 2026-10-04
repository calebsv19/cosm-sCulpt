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
in `<workspace>/_private_workspace_artifacts/van_workflow/`. The generator refuses
an existing destination; it never refreshes or replaces a user's working file.

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

## Mouse-driven walkthrough

1. Open the working layout using **File → Load Layout**. Select **Measure → Units
   → mm**. Select a named view, then **View → Fit scene**. Fit changes the camera
   and zoom only; it uses visible geometry and does not add an undo entry.
2. In **Measure → Saved constraints**, choose `bed_lift`. It selects Travel,
   Z direction, limits **900–1600 mm** and **Moves: Lift bed**. Drag the slider,
   or use Min/Max/Reset. The deck, mattress and two rails move together. Use a
   side or free view to see the height change; Top hides Z movement. One slider
   gesture is one undo step. Undo clears the active measurement form; reselect
   the saved rule. Save after restoring the starting position if Dirty remains.
3. Choose `water_door_hinge`. The saved **0–90°** motion moves the nested Cabinet
   door assembly, including its handle, around the hinge reference. Reset returns
   it to the saved home. **View envelope** navigates to the derived overview box.
4. Inspect `controller_height`: the controller origin stays **570 mm above the
   pump origin along Z**. This is distinct from the saved **100 mm minimum surface
   clearance** check. Inspect `bracket_right_angle` for coincident offset pivots
   and a maintained **90°** planar angle.
5. In **Parts → Assemblies**, inspect the root, OEM reference, Lift bed, Water
   cabinet and its nested Cabinet door. In **Parts → Objects**, inspect names,
   types, IDs, membership and properties. In **Parts → Links**, inspect the bed
   support links, cabinet/pump attachments and controller/tank/battery containment.
   Containment and support links describe intent; they do not prove fit or strength.
6. Use **Parts → Filters** to inspect a subsystem or Reference/Design subset.
   Clear the filter before the overall audit. Hidden objects remain in spatial
   validation. The controller's `network=can0` and power-domain fields are labels;
   there is no routed cable or verified electrical topology yet.
7. In **Parts → Volumes**, inspect the walkway, fuse-box service space and the
   two labeled motion overview boxes. In **Parts → Checks**, run checks, select
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
Desktop interaction and packaging readback are recorded separately at closeout;
passing tests alone do not establish user acceptance.

The initial real workflow exposed an overly restrictive legacy zoom cap: with a
100 mm grid, the van could remain tiny. **Fit scene** and a physical zoom ceiling
now make meter-based geometry navigable. Scene fit is explicit, not automatic.
Narrow selectors still clip long names; automatic/saved warnings need grouping.
The teaching model does not validate materials, load capacity, manufacturing
clearances, whole-van occupancy, electrical compatibility or real measurements.
Independent moving followers, imported moving meshes and simultaneous joints
remain outside the bounded motion contract.

## Next implementation boundary: S5

First retain this document as a repeatable acceptance project and refine the
selector/check-result readability. Then implement S5 as small UI-first slices:

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
