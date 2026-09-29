# Persistent Geometric Constraints

Status: S1c delivered; initial S1d coincident/planar-mate rules delivered.
Date: 2026-09-28

## User workflow

In the Scene workspace, open the right-hand **Object tab → Measure**, choose A (driver) and B (dependent), and select origin,
local axis or named primitive face references. D/C still perform one-time placement.
The persistent controls are separate:

| Key | Operation |
|---|---|
| R | Save a signed projected distance along the P-selected world axis; enter `20 mm`, `0.5 m`, etc. |
| O | Save coincident reference points; confirm with Enter |
| M | Save coincident points plus a signed relative angle about the N-selected plane normal; enter degrees such as `30` or `90` |
| Q | Cycle existing saved rules, then back to new-rule mode; chosen operands and stored axis/normal are shown |
| Delete | Confirm removal of the Q-selected rule; geometry stays in place |
| Enter / Escape | Apply / cancel the active rule entry |

A selected rule is replaced by R/O/M, retaining its ID. Choosing another object or
feature returns to new-rule mode. For target-only edits, the selected rule retains
its stored axis/normal, including oblique axes authored through the C API. P/N
explicitly chooses a replacement axis/normal. Length suffixes override the display
unit; bare distances use the display unit. Angles use degrees in (-180,180].

Example: choose a cabinet's +U face as A and another panel's -U face as B, choose
world +X, and save R=`20 mm`. Moving/resizing A updates B to maintain that signed
face-center projection. A is grounded unless another rule drives it. The distance
rule preserves B's other translation components and orientation. Moving B in a
free perpendicular direction remains allowed. A conflicting direct edit of B is
rejected; edit the target or driver instead.

Face references are face centers and normals, not arbitrary surface points.
A coincident rule maintains the selected points but leaves orientation free when
an edit preserves those points. A planar mate aligns those points and rotates B
until its selected direction has the requested angle relative to A. Both selected
directions must lie in the explicit world plane. A driver's in-plane rotation
propagates. Out-of-plane configurations are refused, not silently projected.
This is a fixed mate; it does not yet define hinge travel, rails or motion limits.
Coincident points and mates can overlap solids: there is no collision check here.

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

Layout schema **11** requires `geometricConstraints` and `nextConstraintId`.
Each rule stores ID, kind, A/B references, world vector and target (meters for
projected distance, degrees for planar mate). Object identity, physical context,
frame and display-unit contracts remain unchanged. Layout schemas 0–10 remain
readable. Older readers reject 11 rather than silently discard rules.

Loading validates the graph and stored geometry without silently solving stale
input. An invalid import leaves the current document untouched. A malformed
embedded authoring snapshot cannot fall back to geometry-only import. Valid
whole-document replacement may replace its rule set just as it replaces geometry.
The separate object-asset authoring format cannot store scene rules. Rule creation
in the Object workspace is refused, and saving a constrained layout as an object
asset fails before touching its destination file. Save it as a scene instead.

The canonical authoring export retains schema 11 in its embedded layout snapshot;
runtime compilation consumes the solved geometry. The renderer is not a constraint
solver. Exporting invalid geometry fails. A physical-scale export override that
makes the saved targets inconsistent also fails instead of changing their meaning.

## Verification and continuation

`make test`: 425 tests across 43 reported suites, including 16 constraint tests.
Coverage includes actual mouse-drag propagation/refusal, single-gesture undo,
reordered chains, target changes, cycles/multiple drivers, bounds/locks, every
primitive mutation API, failed history reservation, malformed rules, identity
collision prevention, atomic import, undo/reopen/export, UI rule lifecycle, and
coincident/30/-30/180-degree mates including angle wraparound on reload.
The agent-scene producer smoke suite passes. The separate 25-test primitive
resize suite, shape-tool build, and isolated Main Edit package self-test also pass. Repeatable UI fixtures render a
saved 20 mm face gap and a saved 30-degree mate; both were visually inspected:

```
make visual-artifact-constraints VISUAL_ARTIFACT_PATH=/tmp/distance.bmp
make visual-artifact-constraints CONSTRAINT_VISUAL_MODE=constraint-angle VISUAL_ARTIFACT_PATH=/tmp/angle.bmp
```

Next: extend S1d with authored point/axis datums for hinges away from primitive
origins/face centers and explicit pivot feedback. Then S1e adds bounded translation
and angular travel, sampled poses and envelope validation. Keep these distinct
from the fixed mate delivered here. Semantic organization and assemblies follow
after this mechanical-layout foundation has been exercised on a measured van scene.
