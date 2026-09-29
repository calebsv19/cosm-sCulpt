# Geometric References, Measurements and Exact Placement

Status: measurements and one-time placements implemented; persistent rules are now available in the [constraint contract](geometric_constraints.md).
Date: 2026-09-28

## Use in the editor

Open the right-hand **Object** tab and choose **Measure** in Transform.
The dialog starts with the selected primitive as A and the next primitive as B.
Both operands may refer to the same object. Use Tab to select A/B, Up/Down to
choose an object, and Left/Right to choose its origin, +U/+V/+N axis, or named
primitive face. Clicking the left/right halves of an operand row cycles its
object/feature. P changes the world projection axis; N changes the signed angle
plane. Enter, Escape or the Close row closes the dialog.

Press K (or click its row) to enter viewport Pick mode. The active named feature
appears as a square marker on each eligible primitive. Click within 12 logical
pixels to assign that stable reference to A or B; Tab switches operands. Enter or
Escape returns to the measurement dialog. A miss leaves the operand unchanged.
Markers are explicitly X-ray datums, not visible-surface picks. Hidden,
unselectable, unsupported, offscreen and banner-covered candidates are excluded.
Shared core_screen_pick ranks nearby markers by distance, depth and stable object
handle; the selected operand stores the persistent entity ID and feature.

Pick mode draws A/B crosses, a connecting guide when both endpoints are onscreen,
and their physical point distance in display units. Keys 1/2/3 select world XY,
YZ and XZ orthographic views. These are view changes only: construction plane,
geometry and history remain unchanged. Viewport pan/orbit/drag is reserved while
picking; return to the normal editor to reposition the camera.

Inspection is read-only. Explicit placement commands below can change geometry.
The modal consumes other editing/viewport input while active. Opening it again
starts a new measurement selection. Reference operands resolve current geometry
on each evaluation; no stale cached measurement is treated as authoritative.
Empty scenes ask for a plane or prism. Unsupported operands report a reason.

## Exact one-time placement (S1c prerequisite)

Choose A, B and the projection axis, then press **D** (or click the left half of
its action row). Enter a signed length such as `20 mm`, `-0.5 m` or `3.5 in`.
Without a suffix, the current display unit applies. Enter or clicking the target
row applies; Escape or the Cancel row cancels. Backspace edits the input.
References and axes remain frozen during entry; errors keep the entry open.

A stays fixed. B translates along the selected world axis until the signed
projection of B-minus-A equals the target. Its perpendicular position, orientation
and dimensions remain unchanged. Named face references use face centers. Negative
targets put B on A's negative-axis side; this does not check collision or clearance.

Press **C** (or the right half of the action row), then Enter to align B's reference
point to A in all three axes. This aligns points only; it does not align normals or
establish a hinge. A and B must belong to different primitive objects.

Both commands reserve a single undo snapshot only after validating a candidate.
Bounds clamping, plane locks, locked B, invalid references and insufficient numeric
precision refuse the edit. Failure/no-op preserves document, dirty state and
undo/redo. Geometry persists through save/reopen and canonical runtime export.
Later edits can change the gap: **these commands create no persistent constraint**.
A may be locked because it is only read. Non-unit primitive instance scales remain
unsupported. Length tolerance is `1e-6 + 1e-7 * abs(target_meters)`; coincident point
components use 1e-6 meters. Large world coordinates can cause a precision refusal.

The structured C entry point is `Editor_ApplyReferencePlacement` in
`src/Editor/editor_reference_edit.h`. It accepts stable references, kind, world
axis (normalized by the command) and target meters. It reuses the candidate-only
`Editor_PrepareNumericEdit` boundary and re-resolves the resulting feature before
publishing. This is an internal synchronous command, not a new MCP endpoint.

Proof: five ReferenceEdit tests cover face gaps, signed/scaled/oblique placement,
point alignment, bounds/plane/object-lock failures, no-op/redo preservation, large
coordinate precision rejection, undo/reopen/runtime export and modal input.
`make test` passes 409 tests across 42 reported suites (including folder picker).
`make visual-artifact-placement VISUAL_ARTIFACT_PATH=/tmp/placement.bmp` renders a
disposable entry fixture; the 20 mm entry was visually inspected. No shared-module
API/version changes: core_units and existing primitive/reference math are reused;
application-specific command/history policy stays in the app, with publication
now routed through the Layout transaction.

## Persistent-rule follow-up

S1c is now implemented at the common Layout transaction boundary, with initial
S1d coincident/planar-mate rules. See [persistent geometric constraints](geometric_constraints.md)
for R/O/M controls, saved-rule selection/editing, enforcement coverage and limits.
D/C remain one-time operations and do not themselves create rules.

## Operand and measurement meaning

`EditorGeometricReference` stores the persistent string entity ID, reference kind,
and named primitive face where applicable. Numeric handles, array positions,
preview triangles and screen coordinates are not persistent operands. The resolver
supports existing plane and rectangular-prism authored frames with unit instance
scale. U/V/N are the authored primitive basis, already rotated by the existing
geometry operations; Euler rotation must not be applied a second time.

| Result | Definition |
|---|---|
| Point distance | Euclidean distance between origins or face centers |
| Signed projection A to B | Dot product of B-minus-A with an explicit normalized world axis |
| Direction angle | Unsigned angle between axis directions or face normals, 0 to 180 degrees |
| Signed planar angle | A-to-B angle about an explicit normal, (-180, 180] degrees; both directions must lie in that plane |
| Signed parallel plane gap | B-minus-A projected onto A's face normal, for parallel/antiparallel infinite face planes |

Plane gap is not minimum surface clearance and does not require finite face overlap.
Axis references have the object origin as their point; face references have a face
center and normal. Origin alone supplies no direction. Length results are meters
internally and converted using core_units to the selected display unit. Angles
are degrees. The UI exposes +X/+Y/+Z projections and XY/+Z, YZ/+X, ZX/+Y angle
planes; the C API accepts explicit world vectors.

Resolution distinguishes invalid, unresolved, unsupported and degenerate inputs.
Deleted/missing/ambiguous entity IDs never retarget another object. Nonfinite
physical context, zero directions, degenerate sizes, nonorthogonal primitive
frames and non-unit instance scales do not produce an apparently valid value.
Direction normalization uses doubles; source geometry remains float-backed.
Orthogonality/coplanarity tolerance is 1e-5 in normalized dot products; parallel
plane tolerance is 1e-10 in `1 - abs(dot)`. Failed measurements return NaN with
status and diagnostic text. These tolerances do not claim physical accuracy.

## Ownership and compatibility

Reuse adopted: core_units for physical/display conversion and existing core_object
identity, core_math-backed primitive frames and face helpers for geometry.
Reuse deferred: a general shared measurement/solver library; the first resolver
and read-only command remain app-owned until a second consumer establishes a
shared need. core_space/core_scene were reviewed; no new coordinate conversion,
scene schema or shared API/version change is required by this slice.

The editor compatibility entry points are `Editor_ResolveReference` and `Editor_Measure` in
`src/Editor/editor_measurement.h`. They are synchronous, read-only operations,
not a revision-checked external agent protocol. They now alias the Layout reference
API. Measurement selections/results are transient; persisted driving rules use
layout schema 11 as documented separately. Tests retain an operand across a
layout round trip to prove its entity/feature identity resolves consistently.
This does not mean saved measurement annotations have been implemented.

## Verification and remaining gates

Full `make test`: 403 tests across 41 reported suites, including seven measurement
cases and the separate four-test folder picker. Coverage includes 3-4-5 distance,
20 mm face gap, 90/30/-30/180-degree angles, out-of-plane refusal, inch-scale
conversion, same-entity operands, deleted/duplicate IDs, stable operands across
reordering/save/reopen/undo/redo, plane-face validity and modal input isolation.
Main Edit package self-test passed. A candidate package was exercised through the
actual Object/Measure UI and displayed a 90-degree U/V angle with unchanged undo
count; final empty-state/contrast polish passed source/package checks. This is
bounded local proof, not broad hands-on CAD acceptance or a published release.

The marker-picking and transient viewport-annotation follow-up is implemented.
Full regression now passes 404 tests in 41 reported suites. The source fixture
`make visual-artifact-measurement VISUAL_ARTIFACT_PATH=/tmp/measurement.bmp`
renders two primitives 3 m apart with A/B markers, guide and converted readout;
the resulting image was inspected. Main Edit package self-test passed.
The fixture exists only in the explicit visual-artifact mode and does not save a
scene. Marker selection tests cover success/miss, nonmutation, stable face kind,
hidden/unselectable exclusion, pane bounds and named-view construction-plane isolation.

Remaining S1b extensions: exact visible-surface picking, independently authored
point datums, mesh feature naming, broader snapping, and saved measurement records
if required. Persistent distance and initial pivot/angle rules now exist in the
separate constraint contract; the numerical D/C commands remain one-shot edits.
