# S5 Routing and Measured Van Plan

Updated: 2026-10-04. S5A polylines plus initial S5B views/corridors and S5C route/electrical checks are implemented. See [current controls and limits](routing_views_and_electrical.md).
See [physical routes](physical_routes.md) for the delivered UI/native/CLI contract
and [narrow van v2](van_layout_narrow_v2.md) for the corrected furniture layout.

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

The original 38-object van remains an illustrative acceptance fixture. The separate
likely-2023 nominal shell now has a corrected narrow-bed furniture revision. Both
remain provisional: published maximum extents do not establish usable steel or
finished-wall geometry. Stable IDs and existing motion/check exercises survive
these dimensional corrections.

## S5A: manual physical routes with a usable editor

The delivered first slice provides one complete route workflow, including
visible controls, persistence, undo, headless inspection and validation. Do not
expand Measure with another large authoring form. The compact Routes tab wraps with the other tabs to retain readable labels.

An initial route needs a stable ID, name, semantic kind (Cable/Pipe), source and
destination entity references, ordered physical points and optional metadata.
Store physical route coordinates in meters. Entity endpoint references may carry
explicit local offsets; expose how they resolve in world coordinates. Start with
world-space interior points and an explicit endpoint-refresh policy. Moving an
endpoint must invalidate/check the route rather than silently imply the entire
cable is safely rerouted. Rename/reparent must preserve references; deletion must
refuse or explicitly detach referenced endpoints.

Original acceptance direction (delivered scope and limitations are in the contract):

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

### Opening slice: views before more wiring

First add saved semantic view definitions and independent visibility toggles to
the existing View/Parts UI. The current Parts filter supports one AND query,
is transient and excludes physical routes. It does not yet provide simultaneous
layer toggles. Include routes in the semantic query/visibility contract before
adding corridor membership; preserve their stable IDs and undo behavior.

The initial view controls should be short, individually labeled toggles:
OEM, Furniture, Devices, Wiring, Plumbing, Walkways, Keep-outs, Service and Motion.
Allow several together and offer Show all / Isolate. View membership comes from
types/properties/relationships, not copies of objects or assembly paths. An
object can participate in more than one view. Unknown classifications remain
visible in All and are explicitly discoverable.

Save query definitions in the scene; keep the user's active display choices
separate from authored object visibility and engineering validity. Hiding a view
must affect viewport drawing, annotations and picking consistently, including
route overlays. It must not remove geometry from save/export or obstacle checks.
The route editor should explain when a selected route is hidden and provide Show.
Reopen/Undo must preserve definitions without silently changing physical geometry.

Acceptance: isolate the upper wiring, combine it with bed motion and walkways,
toggle furniture separately, then Show all. Confirm hidden obstacles still produce
the same validation findings and save/reopen retains every entity and route.

### Corridor authoring follows views

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

Electrical route details need a separate bounded contract and visible form:
source/destination, circuit, power domain, nominal voltage, DC/AC/signal class,
wire count, gauge or cross-section, material, protective device and allowances.
Keep unspecified values explicitly unknown. The AI render's 12 V label and the
earlier conceptual 24 V examples are not confirmed electrical decisions.
Provide views by circuit and voltage class once those fields are authored; a CAN
label alone does not establish a power route or compliant bus topology.
Resistance/voltage-drop results also require current, conductor properties,
return-path length and the calculation assumptions. Do not derive those from
appearance or nominal route centerline length alone.

Compiler metadata should reference stable scene entities through the inspected
existing compiler contract. Materials/derived quantities, broader agent operations,
simulation adapters and telemetry bindings follow as consumers of the same spatial
model. They are not implemented merely because this document describes them.

Next coding boundary: explicit intermediate junctions and split circuit sections, then inventory-driven loading, penetrations and refined motion/OEM clearance checks. S5B/S5C opening controls are implemented; automatic routing and solved electrical topology remain future work.
The measured-shell/reference pass can replace demonstration dimensions as soon as
the blueprint references and verified dimensions are supplied.
