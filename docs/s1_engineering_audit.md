# S1 Engineering Layout Audit

Date: 2026-09-29
Source baseline: `4ccf29f`; audited the Main Edit S1E hinge/refinement change set.
Disposition: bounded S1 mechanical-layout baseline implemented and regression-tested;
new hinge hands-on acceptance remains separate. No S2 implementation in this slice.

## What is usable now

| Area | UI and document behavior | Verification / boundary |
|---|---|---|
| Physical document | Meters internally; explicit right-handed Z-up; display units independent; retained legacy meters-per-world-unit | Scale/identity migration, save/reopen and canonical export tests |
| Numeric layout | Unit-bearing primitive dimensions and positions through existing dialogs | Candidate validation and accepted-only history; unit instance scale required for dimensions |
| References / measurement | Measure A/B selectors, Pick in view, origins, U/V/N axes and named primitive faces; physical local offsets | Distance, signed axis projection and relative planar angle; picks are X-ray primitive references |
| Persistent rules | Distance, Join and fixed Angle actions; saved-rule selector, Apply changes and Remove | Stable operands/IDs, deterministic dependency chains, atomic refusal and undo/reopen |
| Linear movement | Tool Travel, world axis, Min/Max/Position, slider and Min/Max/Reset | Captured transverse separation/orientation; driver translation and downstream propagation |
| Angular movement | Tool Hinge, explicit plane and direction references, Edit pivot offsets, degree fields and movement controls | Stable off-center pivot, captured full orientation, driver translation/in-plane rotation, reset/undo/reopen/export |
| Views / grid | Top (XY), Side (YZ), Front (ZX), Free; Advanced Grid step and Apply grid | Named orthographic presets preserve the construction plane; physical grid step preserves visual scale and participates in history |
| Feedback / form lifecycle | Pivot markers, linear limits or angular range, actual/target status; invalid input next to actions | Read-only rendering; controls refresh from changed saved rules after undo/redo; pending limits/offsets disable movement |
| Physical readouts | Primitive scene-list position/size unit labels, object header position and formatted selected size | Avoid raw storage values masquerading as physical lengths; imported meshes still use asset/bounds workflows |

## Hinge workflow

1. Open **Measure**, select **Tool → Hinge**, and choose A (driver) and B (moving object).
2. Choose directional references and the **XY/YZ/ZX** world plane. Local U is the
   default for new angle/hinge operands. Both directions must lie in that plane.
3. If needed, click **Edit pivot offsets…**, choose A or B, enter local U/V/N
   physical offsets and click **Set offset**. Return from Advanced to the compact form.
4. Enter degree **Min**, **Max**, and **Position**, then click **Create hinge**.
   Creation joins the authored pivot points and positions B at the requested angle.
5. Drag the slider or use **Min**, **Max**, **Reset**. Reset returns to the captured
   creation angle. Use **Apply changes** to save revised limits/position.

The suggested 0–110 degree range and the review fixture are examples, not measured
van geometry. Repositioning/changing a saved hinge's reference, plane or pivot
requires removing and recreating the rule. This preserves its captured frame/reset meaning.

## Evidence

- `make test`: **442 tests**, **43 reported suites**, including **33 Constraints**.
- Hinge regressions cover repeated off-center poses, downstream chains, driver
  translation/rotation, limit/lock/twist refusal with unchanged state, reset,
  single-step undo/redo, non-meter world scale, malformed/schema-13 hinge refusal,
  saved geometry validation and canonical authoring/runtime compilation.
- SDL mouse-event tests cover Hinge selection, degree entry, creation, movement,
  invalid input, slider gesture history, compact-pane visibility, pivot access,
  views, grid history and form refresh after undo. Existing render read-only tests pass.
- `make agent-scene-smoke` passes; `make shape-sanity` builds its tool. The standalone
  shape asset check is not a gate for S1 and its default `config/objects` inputs
  are absent in this checkout. Main Edit package self-test passes.
- [Native-rendered hinge fixture](assets/s1-hinge.png) inspected: full compact form,
  degree fields, slider/buttons, green range arc and actual/target feedback.
- A disposable native app loaded the saved hinge via its catalog. Subsequent native
  clicks failed with the computer-control error `noWindowsAvailable`; screenshots
  and the app process remained available. This is not a completed native mouse walkthrough.
- Earlier compact Measure/linear-travel usability was confirmed by the user.
  The new Hinge controls have not yet received that human acceptance.

## Remaining S1 scope limits

The resolver and driving rules support planes/rectangular prisms at unit instance
scale, up to 32 rules, one incoming driver per dependent and an acyclic graph.
Hinges are single-plane, with ordered limits in **(-180, 180]** containing current
and reset positions. Translation/twist beyond the captured hinge frame is refused.
An ordinary center-based rotation of an off-center hinged part is refused when it
moves the pivot; use the slider/Position to rotate about the authored pivot.

Bounds may conservatively refuse an intermediate rotation even if a later
translation would fit. Float geometry tolerances are acceptance tolerances, not
manufacturing accuracy. Hidden entities remain part of rule validation; hidden
participants are omitted from annotations. Range guides are not swept solids.

Broader original S1 ambitions remain extensions: arbitrary mesh vertex/edge/surface
snapping, independently named reusable datums, arbitrary local-axis authoring,
general mechanical mates/closed loops, and verified perspective preview. Free
view is not presented as proof of perspective rendering. Existing grid snapping
is retained. No collision, minimum solid-surface clearance, clipping/sections,
motion-envelope or free-space analysis is claimed. A dedicated external motion
command/revision-checked agent batch is also future work.

## Next bounded work

S2 follow-up is now implemented: [Parts, semantics and rigid assemblies](semantic_assemblies.md).
The opening boundary below records the S1 handoff; S3 is next.

Original S2 handoff: start **S2 with semantic organization**: stable typed categories, extensible
properties, reference/design designation, and a small query/filter UI on the same
saved entities. Prove save/reopen/export/undo and visible filtering before nested
transform assemblies. Then add an acyclic parent tree with reparent/move tests
that preserve IDs and keep transform parenting distinct from relationships.

Relationships plus reserved/service volumes and structured checks follow in S3.
S4 adds sampled poses and conservative envelopes built from these saved movement
rules; a green range guide alone cannot answer whether a cabinet blocks a door.
Measured van boundaries and component dimensions can meanwhile be entered using
S1. No van capture, mechanical safety or as-built dimensional accuracy is implied.
