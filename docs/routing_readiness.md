# Routing readiness

Updated: 2026-10-04. This is a preparation boundary, not a claim that the wiring
design or all view controls are finished.

The separate editable desktop document is `van_routes_working.json` in
`~/Library/Application Support/LineDrawing-Main-Edit/projects/van_workflow/`.
It begins from the corrected narrow ProMaster v2 scene with the S5A manual route.
The earlier illustrative, concept, nominal, narrow-v2 and routing example files
remain reference snapshots. Do not regenerate over the working document.

Use **Routes** for the Cable/Pipe polyline, endpoint choices, ordered point editing
and length. **Save route** accepts one undoable edit; **File > Save Layout** writes
the active working document. Use **Measure > Saved constraints > bed_lift** to move
the whole narrow bed through Z=950–1650 mm with the slider or Min/Max/Reset.
Reset returns to the saved home. Use a free/side view to inspect vertical movement.
**Parts > Filters** can isolate existing geometry by type, assembly, role or
property. **Show all** clears that transient filter. It currently excludes routes.

The next implementation sequence is:

1. Saved query definitions with independent, concise view toggles for OEM,
   furniture, devices, wiring, plumbing, walkways, keep-outs, service and motion.
   Extend query/drawing/picking to physical routes. Hidden objects remain obstacles.
2. Named upper halo/risers and other explicitly authored corridors, with visible
   route membership and separate corridor geometry. Membership is intent until checked.
3. Segment-level checks against reserved/service/motion space, radius/clearance,
   stale inputs and corridor boundaries, with clickable evidence and Undo/reopen proof.
4. An explicit electrical-details form and circuit/voltage views. Unknown voltage,
   gauge, current or protective device stays unknown; calculations wait for their
   required inputs and assumptions.

The narrow bed and four-section proportions are authored intent. OEM shell boxes,
sleeping length, lift heights, corridor positions and the example route are still
provisional. Use the [measurement checklist](van_measurement_checklist.md) before
treating the model as a build plan. See the [S5 plan](s5_routing_plan.md) for the
contracts and acceptance criteria, and [physical routes](physical_routes.md) for
the delivered controls and agent interface.
