# Geometric References, Measurements and Exact Placement

Status: measurements and one-time placements implemented; persistent rules are now available in the [constraint contract](geometric_constraints.md).
Date: 2026-09-28

## Use in the editor

Click the right-hand **Measure** tab. The existing Object-tab Measure button also
opens it. The pane stays beside the viewport; there is no modal shortcut sheet.
Selections survive switching tabs. Scene/object creation and editing retain their
existing tabs. Scroll within Measure when the window is short or a section expands.

1. Click the A/B object selector to choose a primitive from the list.
2. Choose **Center**, a local axis, or a named face from its feature selector.
3. Alternatively click **Pick in view**, then a displayed reference marker.
4. Read distance, signed axis projection and signed planar angle. Use the visible
   axis and plane buttons to choose world directions. **More measurements** expands
   unsigned direction angle and signed parallel-plane gap.

A is blue and B orange in the pane and viewport. Picking uses the chosen feature
and local offset on each eligible primitive, with a 12-pixel capture radius.
Hidden, unselectable, unsupported and offscreen candidates are excluded. Markers
are X-ray references, not visible-surface picks. A miss preserves the reference;
a successful click finishes picking. Click **Cancel pick** to disarm it. Camera
pan/zoom remains available, and leaving the tab cancels picking/text focus.
core_screen_pick retains deterministic distance/depth/stable-handle ordering.

Opening Measure does not capture the keyboard. Clicking a target field activates
text entry and selects the previous value for replacement. Enter finishes target
entry; the scene changes only when an explicit action button is clicked. Escape
or **Cancel input** ends entry. Lengths accept unit suffixes such as `20 mm` and
`3.5 in`; bare values use the displayed unit. Angles are degrees.

## Position and constrain

Choose **Distance**, **Join**, or **Angle**. A stays fixed and B moves.

- **Distance → Move once** changes the signed axis projection to the entered
  target. **Save rule** keeps that distance through later supported edits.
- **Join → Join once** makes the reference points coincide. **Save rule** keeps
  them coincident. No target scalar is needed.
- **Angle → Save rule** joins the reference points and maintains the signed angle
  in the selected world plane. This is a fixed planar mate. There is no one-time
  angle operation in this slice; its Move once button is disabled.

Unsupported references and same-object operands disable mutation actions. Rule
saving requires Scene workspace. Geometry errors appear in the pane. Failed edits
preserve geometry and undo/redo. One-time placement uses
`Editor_ApplyReferencePlacement`; persistent rules use the existing atomic Layout
transaction. These are application C operations, not a new MCP transport.

Expand **Pivot offset**, choose Reference A or B, click a local U/V/N field,
enter a physical length, and click **Set offset**. **Reset offsets** zeros all
three staged components. Save/update a rule to persist offsets. The definition and
resize/rotation behavior are in the [constraint contract](geometric_constraints.md).

Open **Rules** to select an existing rule. Its operands, operation, world vector,
and target populate the form. Click **Update rule** to replace it under the same
ID. **Remove** presents a confirmation; geometry stays in place. **New rule**
leaves the selected operands available for a separate rule.

## Verification

432 tests across 43 reported suites pass. Two mouse-event regressions cover object
and feature selectors, typed targets, move-once, rule creation/update/removal,
confirmation/cancel, undo, offset controls, scrollbar routing, tab switching,
reference retention and focused keyboard capture. Existing geometry, import,
constraint, reference-picking and editor tests continue to pass. Native pane
fixtures were visually inspected at the compact pane width. The isolated Main
Edit package self-test passes. No shared module API or scene-schema changes.

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
layout schema 12, including local-meter reference offsets, as documented separately. Tests retain an operand across a
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
