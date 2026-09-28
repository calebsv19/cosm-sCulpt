# Geometric References and Measurements (S1b reference portion)

Status: implemented; driving constraints and remaining navigation work are pending.
Date: 2026-09-28

## Use in the editor

Open the right-hand **Object** tab and choose **Measure** in Transform.
The dialog starts with the selected primitive as A and the next primitive as B.
Both operands may refer to the same object. Use Tab to select A/B, Up/Down to
choose an object, and Left/Right to choose its origin, +U/+V/+N axis, or named
primitive face. Clicking the left/right halves of an operand row cycles its
object/feature. P changes the world projection axis; N changes the signed angle
plane. Enter, Escape or the Close row closes the dialog.

The dialog is read-only: it consumes editing/viewport input while active and does
not add undo entries, change geometry, or save annotations. Opening it again
starts a new measurement selection. Reference operands resolve current geometry
on each evaluation; no stale cached measurement is treated as authoritative.
Empty scenes ask for a plane or prism. Unsupported operands report a reason.

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

The C entry points are `Editor_ResolveReference` and `Editor_Measure` in
`src/Editor/editor_measurement.h`. They are synchronous, read-only operations,
not a revision-checked external agent protocol. No layout schema bump is needed:
measurement selections/results are transient. Tests retain an operand across a
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

Remaining S1b: viewport point/face picking and annotations, explicit authored
point datums, mesh feature naming, named-view/snap improvements, and persistent
measurement records if required. S1c then adds a grounded, directed persistent
distance rule; S1d adds pivot/relative-angle rules. Neither solver nor driving
constraints are implemented here. Existing numerical edits remain one-shot edits.
