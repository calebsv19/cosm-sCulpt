# S5 Routing and Measured Van Plan

Updated: 2026-10-04. S5 is planned, not implemented at this checkpoint.

## Current boundary

The existing scene document remains authoritative. The accepted physical contract
uses meters, an explicit right-handed Z-up frame, independent display units and
stable entity identities. Existing Scene/Object authoring, camera/light paths,
imports and exports remain the foundation.

| Stage | Available in the current bounded workflow | Limits that still matter |
| --- | --- | --- |
| S0 physical document | Physical units, stable identities and numeric spatial data | Legacy coordinates require explicit conversion; display changes do not rescale geometry |
| S1 mechanical layout | Numeric transforms/dimensions, primitive references, distance/angle measurements and rules, named views, grid controls, bounded Travel/Hinge | No general CAD sketch or multi-joint solver; broader vertex/edge/surface snapping remains future work |
| S2 organization | Semantic properties, filters and nested rigid assemblies through Parts | Assembly membership and semantic relationships have different meanings |
| S3 relationships/checks | Attachment/support/containment through Links, reserved/service volumes and structured spatial findings through Parts | Support links do not prove strength; mesh bounds warnings do not prove exact mesh clearance |
| S4 motion | Saved object/assembly motion discovery, compact movement controls, conservative envelopes, interval checks and read-only range inspection | Unresolved intervals stay explicit; simultaneous joints and arbitrary moving meshes remain separate work |
| Pre-S5 continuity | Object selection retained across Undo/Redo, saved-state reconciliation, auto framing on file load, requested folder discovery and visible access failures | Selection/tab/zoom are not a restored session; explicit scans and remembered-file restore are still synchronous |

The van remains a 38-object illustrative acceptance project. Its reference shell
is 1850 x 3650 x 1900 mm, not a measured vehicle. Saved entity IDs and existing
motion/check exercises should survive each later dimensional correction.

## S5A: manual physical routes with a usable editor

The next implementation should deliver one complete route workflow, including
visible controls, persistence, undo, headless inspection and validation. Do not
expand Measure with another large authoring form. A compact Routes pane is the
proposed entry; verify its fit in the existing pane shell before committing to it.

An initial route needs a stable ID, name, semantic kind (Cable/Pipe), source and
destination entity references, ordered physical points and optional metadata.
Store physical route coordinates in meters. Entity endpoint references may carry
explicit local offsets; expose how they resolve in world coordinates. Start with
world-space interior points and an explicit endpoint-refresh policy. Moving an
endpoint must invalidate/check the route rather than silently imply the entire
cable is safely rerouted. Rename/reparent must preserve references; deletion must
refuse or explicitly detach referenced endpoints.

Proposed mouse workflow:

1. New route; choose source and destination from labeled entity selectors or Pick.
2. Add points with explicit viewport picking or unit-aware XYZ fields.
3. Select a point/segment; move, insert, delete or reorder it with visible controls.
4. Read total physical length and endpoint status beside the route.
5. Apply/Cancel a form edit; a drag is one Undo gesture. Undo/Redo restores selection.
6. Save, reopen and inspect the same IDs, points, endpoint relationships and length.

The authoritative geometry starts as a polyline. Smooth/tube appearance is derived
and must not change its engineering length without an explicit modeling contract.
Existing camera/light animation paths must retain their behavior and serialization.

Implementation boundaries: a route module beside Layout's engineering model; the
existing geometry/history mutation boundary for accepted edits; versioned JSON
migration and atomic rejection; structured agent queries/mutations and export of
the route's engineering data. Rendering consumers should receive an explicit
supported representation; metadata export alone is not tube rendering.

Acceptance: create a water-controller-to-CAN-hub route, edit a point by an exact
millimeter amount, verify length numerically, Undo/Redo, save/reload/export and
inspect through the agent lane. Reject non-finite points, unresolved endpoints and
invalid schema without changing the scene/history. Old scene fixtures still load.

## S5B: named routing corridors

Add named corridors such as upper_low_voltage_halo and passenger_sensor_riser.
Represent corridor geometry separately from Cable/Pipe geometry and record route
membership as structured intent. Reuse stable identity, metadata, filtering and
relationships rather than create another layer system.

Provide visible corridor creation/selection, membership editing, isolation and
route highlighting. The initial van route should use the upper halo and passenger
riser. Declared membership is not proof that all segments are inside the corridor.
Manual routing comes first; automatic pathfinding can be added after the corridor
and obstacle contract is tested.

## S5C: route checks

Evaluate segments against reserved, service and motion space, required clearance,
maximum length and (where defined) required corridor containment. Findings must
identify the route, affected segment, obstacle/rule and evidence level. Clicking a
finding should select/highlight the segment and relevant volume.

Reuse the structured validation/reporting boundary. Account for Cable/Pipe radius
when the model provides one. Keep bounds approximation and unresolved motion
coverage distinct from verified non-intersection; expose stale endpoint/envelope
inputs. A polyline corner does not establish a realizable bend radius. Resistance,
voltage drop and material quantities require explicit gauge/material/circuit and
allowance contracts before they can become engineering outputs.

Acceptance: show a route through bed travel as a problem, move it to the halo,
rerun checks, and preserve the distinction between conservative warning, verified
failure, unresolved result and a supported pass.

## S5D: repeatable van acceptance

Extend the existing illustrative walkthrough with one CAN connection and one
water pipe. Exercise endpoint movement, assemblies, corridor filters, visible
length, obstacle checks, Undo and relaunch. Keep the unchanged starting example
beside a separately saved routing example. Attach source/test, export and installed
UI evidence independently; human acceptance remains a separate step.

## Bringing in actual dimensions and blueprints

This can proceed alongside S5A and should precede trusting routes for the real
build. Blueprint image calibration/import is a new bounded capability if the
provided format is not already supported; it is not currently claimed delivered.

For each top/side/front reference, record its source, units, orientation and at
least one verified scale dimension. Establish the agreed vehicle origin and axes.
Use labeled dimensions or manual measurements for authoritative values; a
perspective rendering alone cannot supply reliable physical scale. Keep OEM
references separate from proposed build geometry and mark estimates explicitly.

Replace the placeholder floor/walls/roof first, followed by wheel wells, door
openings, ribs and mounting locations. Preserve entity IDs as geometry changes.
Then revise bed/cabinet/water/electrical placement, regenerate affected envelopes
and rerun checks before routing against the revised shell. Disagreement between
views or measurements must remain visible rather than be hidden by a global scale.

A useful first measured milestone is a correct shell plus wheel wells/door opening,
a dimensioned bed footprint and its verified vertical travel range. Full cosmetic
reconstruction is unnecessary for that milestone.

## After S5

Compiler metadata should reference stable scene entities through the inspected
existing compiler contract. Materials/derived quantities, broader agent operations,
simulation adapters and telemetry bindings follow as consumers of the same spatial
model. They are not implemented merely because this document describes them.

Next coding boundary: S5A, a manually editable physical polyline route with a
usable compact pane, endpoint identity, physical length, undo and persistence.
The measured-shell/reference pass can replace demonstration dimensions as soon as
the blueprint references and verified dimensions are supplied.
