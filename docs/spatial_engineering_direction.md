# Spatial Engineering Direction


Implementation update (2026-09-29): the bounded S0/S1 mechanical-layout baseline
now includes persistent distance/join/fixed-angle rules, physical pivot offsets,
linear Travel and single-plane Hinge movement with limits, slider and reset.
The compact Measure redesign was confirmed usable by the user before this hinge
continuation. Named views, physical grid-step editing, form/history refresh and
visible feedback complete this S1 refinement. See the
[S1 audit](s1_engineering_audit.md) and [constraint contract](geometric_constraints.md)
for proof and exclusions. S2 semantic organization and nested rigid assemblies
now ship through Parts; see [S2 workflows and contract](semantic_assemblies.md).
S3a attachment/support/containment relationships now ship through Links; see the
[relationship contract](relationships.md). S3B/S3C now provide reserved/service boxes
and read-only oriented primitive intersection/clearance checks through Parts; see
[spatial checks](spatial_checks.md). Mesh results remain bounds-based warnings.
S4 now adds explicit rigid assembly scope to saved Travel/Hinge, whole-group
conservative overview envelopes, member-interval checks and read-only pose previews.
Check full range performs bounded adaptive separation/failure analysis between
samples, keeping unresolved intervals explicit. See [motion envelopes](motion_envelopes.md).
The illustrative van workflow audit now exercises assemblies, service spaces and
motion checks; see [the walkthrough](illustrative_van_workflow.md). Its Z-up screen
projection/picking and readability gaps are the next gate before S5 routing
corridors/cable paths. Independent
constrained followers, arbitrary moving sets, meshes and multiple joints remain
separate contracts. Broader snapping and
reusable datums remain extensions. User acceptance remains separate from regression
and rendered-UI proof.

Status: roadmap accepted; opening CAD slices implemented as noted above, later phases proposed
Date: 2026-09-28
Inspected source baseline: `c8f5bf1` (program VERSION `0.4.0`)

## Product goal

Evolve Sculpt into an agent-editable spatial description of engineered physical
systems. The first proving project is a camper-van interior. Keep the contracts
generic enough for rooms, machinery, robotics, electronics and simulation scenes.

Every meaningful entity should answer: what is it, where is it, what is it
connected to, and what is it allowed to do? Geometry, semantic identity and
relationships/constraints are separate authored concerns. Rendering meshes,
collision approximations and preview geometry are derived representations.

The near-term outcome is an accurate and convenient layout tool: create a
simplified interior, place dimensioned components, measure gaps, inspect views,
organize reference and design objects, save/reopen, and export for rendering.
Real vehicle dimensions have not been supplied. Example geometry must be labeled
illustrative and must not become verified OEM dimensions.

## Accepted foundations and initial CAD priority

The user accepted canonical meters, right-handed Z-up, independent display units,
explicit legacy conversion, stable entity IDs independent of labels/assembly paths,
and separation of transform hierarchy from other relationships. ID encoding,
precision, tolerances and migration details still require bounded design.

The first CAD milestone now includes persistent distance and relative-angle rules
before broad semantic organization or assemblies. Distinguish a measurement, a
one-time numerical edit, and a stored driving dimension. Start with existing
primitives, explicit grounded/movable sides, stable point/axis/face references,
and a chosen plane/axis/pivot. A 500 mm gap or 90-degree connection must remain
true after supported edits, undo and reopen. An exact edit must report a conflict
if bounds adjustment or another rule prevents the requested result.

Prove directed, bounded constraints before attempting a general solver. Reject
unsupported cycles, ambiguous movement and conflicting rules without partial
changes. All mutation entry points for constrained objects must participate in
the same evaluation/undo boundary. A fixed relative angle and a permitted angular
range are separate behaviors; simple edit limits can follow the fixed-angle proof.
Bounded rigid motion/envelopes now ship through S4; independent joints and animation remain later work.

## Existing foundation and gaps

| Area | Inspected implementation | Remaining engineering boundary |
|---|---|---|
| Units | `core_units` supports m, cm, mm, in and ft; numeric dimension/transform dialogs exist | Prove consistent scale from import through editing, measurement and export; unit-bearing input and precision policy |
| Coordinates/view | Local transforms, construction planes, plane/free views, grid and constrained gizmos | Explicit document frame, nested assembly frames, convenient engineering presets, measurement and expanded snapping; perspective preview needs separate verification from exported camera metadata |
| Identity | Scene/object IDs, local numeric handles, mesh asset IDs, separate labels in some authoring records | Define immutable entity identity across rename, clone, reparent, Save As and external references |
| Documents/assets | Scene and Object workflows, authoring/runtime split, reusable imported/runtime mesh assets and portable package export | Add engineering meaning without replacing these lifecycles or editing generated meshes as truth |
| Semantics | Object kind/flags and JSON extension preservation seams | Typed properties, categories, semantic queries, saved views and assemblies |
| Editing/agents | Layout snapshot undo/redo; deterministic request-to-scene CLI and summaries | Shared document command boundary, revision-checked edit batches, complete undo coverage and bounded queries |
| Integration | Shared scene compiler and scene-project exchange with renderer/simulator consumers | Preserve entity/frame/unit meaning through consumer projections; later compiler and telemetry adapters |
| Engineering analysis | Primitive/bounds validation and geometry helpers | Relationship graph, design rules, motion/service volumes, routing and derived quantities |

Source entry points: [layout types](../src/Layout/layout.h),
[unit-aware dialogs](../src/UI/panel/ui_panel_dialog_logic.c),
[UI conversions](../src/UI/panel/ui_panel_controls.c),
[scene import](../src/Tools/scene_import.c),
[canonical export](../src/Tools/canonical_scene_export.c),
[history](../src/Editor/editor.c),
[agent producer](../src/Tools/agent_scene_tool.c), and
[workspace handoff](../src/Core/workspace/line_drawing_workspace_mode_handoff.c).

## Architecture direction

1. Preserve one authored scene authority. An Engineering workspace, if useful,
   is a task-oriented view over the same document and command service. Retain
   Scene arrangement and Object asset editing; do not fork a second geometry
   database or start a UI rewrite.
2. Use the accepted canonical meters and explicit right-handed Z-up frame, consistent
   with existing shared contracts. Display units are presentation. Existing
   scene scales and legacy frames require explicit compatible conversion.
3. Keep immutable IDs separate from labels, assembly paths and geometry assets.
   Human-readable paths may be aliases. A hierarchy move must not invalidate a
   compiler or relationship reference.
4. Add typed, namespaced semantic properties, one acyclic transform-parent tree,
   and a separate typed relationship graph. Electrical connectivity and support
   do not imply transform inheritance.
5. Use the existing authoring schema and extension seams only after proving
   round-trip preservation. Add a versioned engineering contract; whether it is
   a namespaced extension or an atomic sidecar is an implementation decision.
   A sidecar must share document revision, save, undo and export boundaries.
6. UI and agent operations should invoke the same validated document commands.
   Begin with cheap inspect/query and one reversible numerical edit. Add batch
   transactions, revision checks and structured diagnostics as the command set
   grows. MCP is an optional transport, not the model's owner.
7. Treat display visibility separately from physical existence. Hidden design
   entities still participate in validation unless an explicit analysis policy
   excludes them. Ghosted references remain reference evidence.
8. Compile portable scene subsets for rendering and simulation. Preserve source
   entity IDs and report unsupported engineering semantics rather than silently
   dropping them. Keep derived state tied to document revision and inputs.

## Delivery order

| Stage | Usable result | Acceptance example |
|---|---|---|
| S0: physical/document contract | Agreed units, frames, IDs, document authority, precision and migration rules | A scale/frame/identity fixture matrix and the first command contract are reviewable |
| S1: initial CAD layout | Numerical placement/dimensions, measurement/references, orthographic/grid behavior, then persistent distance/angle constraints and bounded edit limits | Enter `3.5 in`, read `88.9 mm`; preserve a 500 mm gap and 90-degree/30-degree connection through driver edits, undo, reopen and export; conflicts leave state intact |
| S2: organized van mockup | Basic semantic categories/properties, reference/design visibility queries, nested assemblies and panels/beams/volumes | Move a cabinet assembly, preserve children and IDs, isolate electrical objects, render the same revision |
| S3: relationships and checks | Attachments/support/connectivity, reserved/service volumes, a structured validator | A cabinet intruding into a declared door clearance is reported with entity IDs and measured evidence |
| S4: motion | One-axis sliders and hinges, sampling and conservative envelopes | A cabinet blocking any part of bed travel is reported even when the bed currently clears it |
| S5: routes and quantities | Manual cable/pipe routes, corridors, endpoints, lengths and provenance-aware material quantities | A routed cable has reproducible length and reports corridor/bend-rule violations |
| S6: compiler bridge | Scene entity manifest joined to existing semantic/physical/hardware graphs | Missing or stale entity mapping fails deterministically without inventing compiler syntax |
| S7: runtime twin | Separate timestamped telemetry bindings and revision checks | Stale or unmatched observations are visible and never overwrite authored design |

Agent operations, validation, undo, migration tests and consumer compatibility
are developed throughout S1-S7, rather than deferred to a final agent phase.
The stages describe dependency order; each is divided into smaller proven slices.

## First van workflow

Start with a deliberately simple illustrative interior: floor, two walls, roof,
wheel-well boxes, bed, cabinet, battery/tank proxies and a walkway volume. Add
actual ribs, mounting points and door openings as measurements arrive. Mark each
measurement as unknown, estimated, manufacturer-derived or measured, with source,
units and uncertainty. Missing dimensions remain unknown, never zero by default.

Keep imported scans as optional reference geometry. Register them against known
measurements and retain their source/scale provenance. The engineering entities
can remain simple while the scan supplies visual context. Save a renderable
checkpoint early and improve fidelity only when the task requires it.

## Deferred work

No immediate full parametric CAD kernel, general mechanical mating solver, FEA,
CAM, BIM, automatic wiring optimizer, photogrammetry reconstruction system, or
photorealistic renderer inside Sculpt. OBJ/LiDAR ingestion and advanced booleans
need separate capability audits. Existing STL/runtime-mesh support is not proof
of those formats or engineering solids.

Support graphs are authored design intent, not structural certification. Material
appearance is separate from density/resistivity/strength evidence. Approximate
collision or motion results must state their method and error bounds.

This direction is refined through bounded implementation proposals. See
[current truth](current_truth.md), [future intent](future_intent.md), and the
[Main Edit runbook](main_edit_worktree.md) before starting source work.
