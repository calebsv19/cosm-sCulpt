# Persistent Geometric Constraints

S4 continuation (2026-10-02): saved Travel/Hinge rules may explicitly select a
containing rigid assembly through Measure's **Moves** control. The slider/numeric
position moves its design primitive members and nested frames together, with
atomic lock/bounds/conflicting-rule refusal. Schema 19 stores `motionAssembly`;
empty remains B-only. Full-group envelopes and read-only obstruction previews use
the same movement path. See [scope, controls and limits](motion_envelopes.md).


Status: bounded S1c/S1d fixed rules/offsets and S1e linear/angular movement delivered.
Date: 2026-09-29

## User workflow

In Scene workspace, open the right-hand **Measure** tab. Choose A (driver) and
B (dependent) with object/feature selectors or **Pick in view**. The viewport stays
available beside the controls. No custom keyboard shortcut is needed.

| Control | Operation |
|---|---|
| Distance + target + Create distance | Maintain signed distance along the selected world axis |
| Join + Create join | Maintain coincident reference points |
| Angle + target + Create angle | Join pivots and maintain signed relative angle in the selected world plane |
| Travel | Create Min/Max/Position in physical lengths, then use slider or Min/Max/Reset |
| Hinge | Create Min/Max/Position in degrees about authored pivots in an explicit plane |
| Pivot offset | Expand local U/V/N fields; Set offset stages a physical offset |
| Saved constraints | Choose a saved rule and populate the form |
| Apply changes | Replace the selected rule under the same ID |
| Remove | Confirm deletion of the rule, leaving geometry in place |
| New | Return to creation mode with the chosen references |

**Move once** / **Join once** are separate one-time placement actions. Changing an
object/feature returns to new-rule mode. A chosen rule retains its stored world
axis/normal, including oblique API-authored vectors, until a visible axis/plane
button selects a replacement. Length suffixes override display units; bare values
use display units. Angles use degrees in (-180,180]. Target entry alone does not
edit geometry; click the action button. See [Measure controls](geometric_measurements.md).

Example: choose a cabinet's +U face as A and another panel's -U face as B, choose
world +X, and save a Distance rule with target `20 mm`. Moving/resizing A updates B to maintain that signed
face-center projection. A is grounded unless another rule drives it. The distance
rule preserves B's other translation components and orientation. Moving B in a
free perpendicular direction remains allowed. A conflicting direct edit of B is
rejected; edit the target or driver instead.

References start at primitive origins or face centers. Each operand now carries
an optional physical `local_offset_meters[3]` in its owning object's U/V/N basis.
The result is an authored point, or an authored axis through that point when an
axis/face direction is selected. Translation/rotation carries it with the object.
Resize/scale does not multiply the offset; a face-based reference still follows
the resized face center. The basis is the object's frame, including for face
references, not a separately oriented face frame. Offsetting a face normal also
shifts its reference plane; measured plane gaps are not solid-surface clearances.

Pivot-offset edits are transient until Save/Update rule commits them (or a
one-time placement consumes them). Select a rule, stage offsets, then Update rule:
geometry and offsets share one undo step. Changing objects resets offsets;
changing features keeps them. The fields display physical lengths. These are
embedded operands, not independently named datum entities. Axis orientation still
comes from U/V/N or the selected face normal.

For an edge pivot, choose Local U axis on both prisms, set A's local U offset to
`0.5 m` and B's to `-0.5 m`, then choose Angle, target `30`, and Save rule. The
points coincide while the primitive centers remain distinct.

A coincident rule maintains the selected points but leaves orientation free when
an edit preserves those points. A planar mate aligns those points and rotates B
until its selected direction has the requested angle relative to A. Both selected
directions must lie in the explicit world plane. A driver's in-plane rotation
propagates. Out-of-plane configurations are refused, not silently projected.
This is a fixed mate; it does not define hinge travel. Linear travel is a separate driving rule described below.
Coincident points and mates can overlap solids: there is no collision check here.

## Linear travel (S1e)

Choose A (rail anchor) and B (moving object), choose **Travel** from Tool, then world **X/Y/Z**.
Set **Min**, **Max**, and **Position**, then **Create travel**. Enter unit-bearing values
such as `900 mm`, `1.8 m`, or `36 in`; a bare number uses the current display unit.
The initial form suggests a one-meter range from the current pose; these are
editable suggestions, not dimensions inferred from the vehicle.

Travel always shows a slider (disabled until creation succeeds) and **Min**, **Max**, **Reset** buttons. The
current numeric position and limits are edited together with **Apply changes**.
Each accepted drag is one undo step; rejected/no-op motion adds none. Reset returns
to the pose captured when the rule was created, not the start of the last drag.
Both the current target and reset position must remain within the edited limits.
Selecting a saved constraint reloads the saved settings; unstaged field text is not authoritative.

The scalar position is the signed projection from A's selected reference to B's
selected reference along a normalized **world-fixed axis**, in meters. Creation
captures perpendicular separation and B's normalized U/V/N orientation. A moving
anchor translates the rail; rotating A does not rotate the rail or B. B can be
translated directly along the rail within its limits, updating the stored target.
Sideways displacement, rotation, out-of-range targets and downstream conflicts
refuse the whole transaction. Existing downstream distance/join/mate rules solve
in dependency order. Existing one-incoming-rule and cycle restrictions still apply.
To change the rail axis/reference/type, remove and recreate its rule explicitly.
Primitive resizing remains possible when the selected reference and orientation
still satisfy the rail; this is a positional constraint, not a frozen solid.

Min/Max markers show the reference travel in the viewport; they are not a swept
solid or a collision result. Hidden participants are omitted. Limits outside the
viewport are not drawn; nearly coincident screen projections stagger the labels.
The bed-lift fixture uses provisional 900–1800 mm travel and a downstream sensor.
No verified van dimensions or structural safety claim is implied.

C callers use `Layout_InitLinearTravel` to capture the rail, `Layout_ConstraintEdit`
to save/update its rule, and `Layout_SetTravelPosition` to move it transactionally.
These use the existing Layout engine; no separate geometry store or runtime solver
is introduced. Shared unit conversion, projection, pane, font and theme APIs are
reused; application-specific rail policy remains in Layout. No shared API change.
The existing agent-scene request format does not yet expose a dedicated motion command.

## Angular travel / Hinge (S1e)

Choose **Tool → Hinge**, A (driver), B (moving part), direction references and
**Plane XY/YZ/ZX**. **Edit pivot offsets…** opens Advanced offsets for an edge hinge.
Both directions must lie in the plane. Enter degree **Min**, **Max**, **Position**
and click **Create hinge**. Bare values or a `deg` suffix are accepted; length
Units do not affect these fields. Ordered limits lie in **(-180,180]** and contain
both position and the captured reset angle. A suggested 0–110 range is only a default.

Creation captures B's complete orientation and A's direction, joins the authored
points, then rotates to Position. The fixed world-plane normal defines positive
relative angle. A translation/in-plane rotation carries B and downstream dependents.
B's full orientation follows the captured frame: out-of-plane tilt, twist or pivot
translation cannot silently become extra degrees of freedom. Use Position/slider
for off-center rotation; an ordinary center rotation that moves the pivot is refused.

The slider and **Min/Max/Reset** are visible before creation and disabled until a
valid rule exists. Each accepted slider gesture owns one undo step. Apply changes
updates limits/position; limits must still contain the original reset angle.
Pending limits or reference offsets disable movement until applied or reset.
Reset restores saved references/settings and creation angle. Changing a saved
pivot/reference/plane requires removing and recreating its rule to recapture the frame.

The green viewport arc and Min/Max labels show a reference range, not an occupied
solid, collision test or motion envelope. `Layout_InitAngularTravel` captures the
hinge; `Layout_SetTravelPosition` accepts **degrees** for angular travel and meters
for linear travel, using the same atomic constraint-edit/history boundary.

## Viewport feedback

Selecting either participant displays its related saved rules in the normal
viewport. Green `OK` or amber `CONFLICT` labels show actual/target distance or
angle; mates also show the residual pivot gap. The read-only
`Layout_ConstraintFeedback` API shares the solver's tolerance calculation.
Successful edit feedback therefore measures committed geometry, while an invalid
attempt leaves that geometry intact and shows the existing refusal banner.

Cross/square markers identify A/B pivots, short rays show reference directions,
and a line joins separate points. Markers are X-ray engineering annotations, not
occlusion-tested surface picks. Hidden participants are omitted; drawing is clipped
to the viewport, and at most five rules plus a count/hint are shown to limit clutter.
Open Measure and use Rules to inspect/edit saved rules. During Pick in view the
selected offset points and their direction rays are visible too.

## Bounded dependency model

- Plane and rectangular-prism references with unit instance scale only.
- Up to 32 rules, stable rule IDs and stable entity/feature operands.
- One incoming driving rule per dependent; a driver may have several dependents.
- Directed chains are sorted by dependency, independently of storage order.
- Cycles, self-rules, duplicate IDs and multiple incoming rules are refused.
- Locked dependents, bounds adjustments, plane locks, unsupported/degenerate
  references and insufficient floating-point precision refuse the whole edit.
- Deleting or cutting a participant requires removing its rules first. Whole
  Object Authoring replacement of a constrained layout is refused. Adding an
  unrelated object is allowed and generated IDs avoid imported-name collisions.

Distance tolerance is `1e-6 + 1e-7 * abs(target_meters)` meters. Coincidence uses
1e-6 meters Euclidean distance; angle tolerance is 1e-4 degrees modulo 360.
These are float-storage acceptance tolerances, not manufacturing accuracy claims.
Mate rotation can conservatively reject an intermediate pose outside active
bounds, even when a later translation could fit it. Arbitrary topology references,
mesh constraints, simultaneous multiple-axis rules, closed loops, and a general
mechanical solver are outside this slice.

## Unified transaction boundary

The Layout transaction stages a copy of the object store, applies the proposed
edit, solves downstream rules, validates the result, reserves history, then copies
accepted object/rule data into the live store. No live object pointers are replaced
by an accepted transaction. Failure/no-op preserves geometry, rules, dirty state,
and undo/redo; a transient refusal message is displayed. Constrained drag gestures
reserve one snapshot on the first accepted increment. Failed/no-op gestures do not
consume history or clear redo.

| Entry path | Enforcement |
|---|---|
| Numeric position/dimensions and one-time reference placement | Candidate preflight, then full-store transaction |
| Primitive position, dimension, rotation and scale APIs | Common transaction wrapper |
| Plane/prism 2D, 3D and depth resize handles | Same wrapper, resolving current feature geometry |
| Mouse translation/rotation/scale/resize | Wrapped APIs plus accepted-only gesture history |
| Rotation dialog | Wrapped API; history follows accepted constrained change |
| Keyboard/list deletion and cut/extrude replacement | Refuse participants before history or partial replacement |
| Object Authoring whole-store evaluation | Refuse constrained layout replacement |
| Save/import/export | Validate persistent rules; reject unresolved or unsatisfied authored snapshots |

The geometry layer remains app-owned. It reuses core_units, primitive frame math,
core_object IDs, and existing history serialization. No shared module API/version
change. Read-only reference evaluation moved to `Layout/layout_reference.h` and
`Layout/scene/layout_reference.c`; the Editor header retains compatibility aliases.
There is no UI or editor-history linkage in headless reference/constraint code.
The desktop registers the history hook; standalone tools can use explicit hooks.

C entry points in `Layout/layout_constraints.h`:
`Layout_ConstraintEdit` (create/update/remove), `Layout_ValidateConstraints`,
`Layout_RunGeometryEdit`, and `Layout_ReplaceGeometryObject`.
An empty rule ID allocates a monotonic collision-checked ID; an existing ID updates
that rule. Supply `Editor_ReserveGeometryHistory` with the editor for typed desktop
commands. Mutation callbacks may only edit candidate object/rule data; they must
not reallocate the candidate store or mutate shared anchor/wall/asset storage.
This synchronous internal interface is not a new MCP transport or revision-checked
multi-command transaction API. Unrelated legacy anchor edits retain their own paths.

## Persistence and compatibility

Layout schema **14** requires `geometricConstraints` and `nextConstraintId`.
Each rule stores ID, kind, A/B references, world vector and target (meters for
projected distance, degrees for planar mate/hinge). Object identity, physical context,
frame and display-unit contracts remain unchanged. Layout schemas 0–13 remain
readable. Schema 11 references without offsets become zero-offset references.
Schemas 12–14 require finite `offsetU_m`, `offsetV_m`, and `offsetN_m` on both
operands. Partial/malformed offsets are refused even in an older document.
Schema 13 adds kind `LINEAR_TRAVEL`, `travelMin_m`, `travelMax_m`, `travelHome_m`,
three-component `travelOffset_m` and nine-component `travelBasis`. These fields are
required for travel; finite ranges, orthonormal basis, transverse offset, graph and
actual geometry are validated. Older readers reject 13 rather than lose motion rules.
Schema 14 adds kind `ANGULAR_TRAVEL`, `travelMin_deg`, `travelMax_deg`,
`travelHome_deg`, three-component unit `hingeReference` and nine-component
`travelBasis`. Its finite coplanar reference, positive orthonormal captured basis,
limits, reset and current solved geometry are validated. Schema-13 hinge records
are refused; older readers reject 14 rather than discard hinges.
The nested `file.schemaVersion` is now read correctly for offset requirements too.

Loading validates the graph and stored geometry without silently solving stale
input. An invalid import leaves the current document untouched. A malformed
embedded authoring snapshot cannot fall back to geometry-only import. Valid
whole-document replacement may replace its rule set just as it replaces geometry.
The separate object-asset authoring format cannot store scene rules. Rule creation
in the Object workspace is refused, and saving a constrained layout as an object
asset fails before touching its destination file. Save it as a scene instead.

The canonical authoring export retains the current layout schema (19) in its embedded layout snapshot;
runtime compilation consumes the solved geometry. The renderer is not a constraint
solver. Exporting invalid geometry fails. A physical-scale export override that
makes the saved targets inconsistent also fails instead of changing their meaning.

## Historical S1 verification and continuation

`make test`: 442 tests across 43 reported suites, including 33 constraint tests.
Travel coverage includes attached chains, direct in-range movement, sideways/rotation
and range refusal, downstream lock/bounds rollback, reset, mixed-unit mouse fields,
single-slider-gesture undo, non-meter world scale, malformed/legacy-schema import
refusal and canonical authoring/runtime export.
New coverage proves meter-valued offsets in a non-meter world scale, resize and
rotation, offset mate propagation, undo/redo/reopen, legacy loading, atomic malformed
input refusal, UI staging/cancel/save, and read-only feedback. Canonical authoring
export/import preserves nonzero offsets. Coverage also includes actual mouse-drag propagation/refusal, single-gesture undo,
reordered chains, target changes, cycles/multiple drivers, bounds/locks, every
primitive mutation API, failed history reservation, malformed rules, identity
collision prevention, atomic import, undo/reopen/export, UI rule lifecycle, and
coincident/30/-30/180-degree mates including angle wraparound on reload.
The agent-scene producer smoke suite passes. The included 25-test primitive
resize suite, shape-tool build, and isolated Main Edit package self-test also pass. Repeatable UI fixtures render a
saved 20 mm face gap and a saved 30-degree mate. The new offset-pivot fixture
shows a saved edge mate and live feedback in the normal viewport:

```
make visual-artifact-constraints VISUAL_ARTIFACT_PATH=/tmp/distance.bmp
make visual-artifact-constraints CONSTRAINT_VISUAL_MODE=constraint-angle VISUAL_ARTIFACT_PATH=/tmp/angle.bmp
make visual-artifact-constraints CONSTRAINT_VISUAL_MODE=constraint-pivot VISUAL_ARTIFACT_PATH=/tmp/pivot.bmp
make visual-artifact-constraints CONSTRAINT_VISUAL_MODE=constraint-travel VISUAL_ARTIFACT_PATH=/tmp/travel.bmp
make visual-artifact-constraints CONSTRAINT_VISUAL_MODE=constraint-hinge VISUAL_ARTIFACT_PATH=/tmp/hinge.bmp
```

Hinge coverage adds off-center repeated poses, driver translation/rotation, range,
lock/twist rejection, reset/history, non-meter scale, malformed capture/schema
refusal, canonical export and actual SDL mouse controls. S1 view/grid/history-form
refinements are also covered. Native hinge rendering and catalog reopen succeeded;
further native coordinate clicks were blocked by the computer-control tool.
See [S1 audit](s1_engineering_audit.md) for proof levels and remaining CAD scope.

Next: S2 semantic organization and nested assemblies. S3 adds reserved/service
volumes and validation; S4 adds sampled poses/envelopes using these motion rules.
Independently named/reusable datums, broader mesh snapping and arbitrary local axis
directions remain extensions. Measured van dimensions can be entered with S1 now.
