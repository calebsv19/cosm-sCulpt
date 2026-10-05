# Sculpt Current Truth

## S5 connected sections, inventory and panel furniture (2026-10-05)

Ordinary Scene outlines now use one stroke without decorative box-face diagonals.
Routes adds collapsed Connections / sections with shared-endpoint navigation and
atomic split-at-point junction creation/reuse. Native readback/export reports
endpoint incidence, stale sections, port offsets and passive domain conflicts;
route checks include conflicting junction domains. Per-section electrical inputs
remain explicit assumptions, with no automatic network-current solver.

Native inventory readback/atomic patching uses existing stable IDs and metadata.
Read-only dimensions are observed from geometry; invalid/duplicate IDs, unsupported
fields and capacity failures reject the entire update. This provides a future
spreadsheet boundary, not a completed Excel importer.

The separate connected van fixture uses hollow panel cabinets/bench/drawers,
contacting desk supports and a joined bed frame, with 130 objects, 21 assemblies,
87 links and 22 routes. The narrow bed dimensions remain 1.80 × 0.82 m and its
six-member motion envelope is regenerated. Power and CAN use distinct junctions;
existing route IDs survive on their source sections. Prior working layouts remain
preserved. Intersections/protection/unknown loading findings remain to be resolved.
See [controls, contract and physical limits](routing_sections_and_furniture.md).


## S5B views, corridors and initial electrical details (2026-10-04)

[Routing views and electrical details](routing_views_and_electrical.md) now ship
through Parts → Views and Routes. Saved query definitions include route membership;
show/hide, Only and Show all control viewport drawing and picking without changing
export or validation. Definitions persist in schema 21; active visibility is transient.
Named editable RoutingCorridor boxes, route membership, radius/clearance and maximum
length are authored through visible controls and participate in Undo.

Check current draft reports whole-segment corridor coverage, primitive intersections,
conservative clearance/mesh/motion findings and stale references. Clicking a finding
selects its target and highlights the affected segment. Reference/OEM geometry is
excluded from this opening route-check slice; current motion checks use overview
bounds, not exact swept geometry. Intermediate junctions and hollow cabinet walls
remain unresolved contracts.

Electrical forms store circuit, domain, voltage, load, conductor area/AWG, material,
explicit return/allowance and tentative fuse intent. Supported copper DC inputs
produce a per-route 20 °C cable-drop estimate; missing inputs stay unknown. Ampacity,
fuse coordination, converter sizing and aggregate network loading are not validated.
The new tentative fixture preserves the narrow bed and adds 13 views and 11 routes
for 24 V, optional 12 V, CAN and local buck branches. Its loads, device sizes and
gauges are explicit comparison assumptions, not selected construction specifications.

514 host tests across 48 groups, native route CLI smoke, warning-clean build and
Main Edit package self-test/refresh pass. The original user working document remains
unchanged; the wiring example is a separate editable file. See the linked contract
for the research basis, precise check boundaries and remaining S5 work.

## S5A routes and routing preparation (2026-10-04)

[Routes](physical_routes.md) now provides manually edited Cable/Pipe polylines,
stable primitive endpoint references, physical length, endpoint freshness,
one-action Undo/Redo, native schema 20 save/reload and structured CLI/export
metadata. Export is centerline metadata, not rendered tube geometry. Camera/light
animation paths remain separate. The corrected [narrow van v2](van_layout_narrow_v2.md)
has a provisional 1800 x 820 mm transverse lift bed and a four-foot entry followed
by three approximately three-foot sections. Real steel/finished dimensions remain
unmeasured; the upper-halo CAN route is an exercise, not validated wiring.

Save As now accepts periods in layout/object filenames, preserving `.layout.json`
rather than silently stripping its dots. Scene package names retain their existing
identifier restrictions. Returning from the editor refreshes a previously requested
home section and its preview cache, selecting the active Save As document. Quick
Actions still performs no directory scan. The document and undo history are untouched
by this catalog refresh. Full host and folder-picker tests pass, including the new
dotted-filename and editor-return regressions.

Toolbar changes to mode, view, plane, bounds or gizmo cancel active input gestures
while retaining Measure's selected rule and the Routes/Parts edit context. They no
longer clear Measure's active state while leaving its controls visible.

At the S5A checkpoint, Parts filters provided one transient AND query for geometry. They did not
include physical routes or saved multi-view toggles. Corridor geometry/membership,
route obstacle checks, voltage/circuit forms and electrical calculations were not
implemented in S5A. The subsequent S5B/C slice above implements their opening
controls; [the plan](s5_routing_plan.md) records the remaining boundaries.

## Pre-S5 continuity and file access (2026-10-04)

Undo/Redo now restores selected objects through stable entity IDs, clearing the
selection if the entity no longer exists. Restored history documents are compared
with the last saved snapshot: returning to the saved pose removes Dirty; returning
to a different pose keeps it. Failed layout/scene/object opens preserve history.
Successful file loads clear history without adding a no-op Undo entry.
File loads use the existing visible-geometry Fit calculation without rescaling the
physical document. Selection, tab and individual zoom/pan state are not persisted.

Folder discovery runs on browser-section entry or explicit folder selection,
not on hover/window events. Unread section counts show an ellipsis. Read failures
produce a visible error and retry guidance. Startup renders a host frame before
restoring the remembered file and skips the surrounding folder scan during restore.
The remembered file read and requested directory scans are still synchronous;
this is not a cancellable asynchronous filesystem worker. Early preference/root
reads also remain. macOS folder selection now uses an app-owned Cocoa panel;
Linux retains its existing chooser/fallback behavior. System privacy grants remain
OS-controlled and are not presumed durable across changed ad-hoc app identities.

Verification: warning-clean build; 504 host tests across 47 reported suites;
4 folder-picker contract tests; agent-scene-smoke and illustrative-van-smoke.
Desktop tests show auto-framed Saved van_working with 38 objects, Bed deck
selection, 900 to 1600 mm whole-bed movement, Undo/Redo with selection retained,
and a slider gesture to about 1248.46 mm followed by one Undo to Saved 900 mm.
Native chooser Cancel and selection of the existing van-project folder pass.
No test pose was saved; the working document SHA256 remains
1e58223a8a3e3248586c8cffce0bc5018c623961275d75fcefc9e299654f40e3.

Next implementation after this historical slice: [S5A manual physical routes](physical_routes.md)
(now implemented), then [views/corridors and routing checks](s5_routing_plan.md).
Actual vehicle dimensions and blueprint calibration
remain pending. This closes the named continuity/file-access slice, not every
CAD, kinematics, filesystem or spatial-query capability in the original direction.

## Saved-motion UI and continuity (2026-10-04)

Selecting a moving object, inherited assembly member or assembly now opens its
existing Travel/Hinge through Measure or Move saved motion. The compact panel shows
saved ID, moving scope, range, slider and Min/Max/Reset; authoring controls expand
explicitly. Travel and Hinge range guides are visible for inherited selection too.
Undo/Redo retains the Measure rule by ID and refreshes fields. Normal-exit app-private
preferences restore units, named/free view, orientation and navigation tool. Parts
labels assembly choices correctly and groups repeated check pairs while preserving
every automatic/saved result and structured report record.

Warning-clean build, 501 host tests / 47 reported suites, both scene smoke lanes and
independent inherited Travel/Hinge rendered fixtures pass. Read-only checking of
the unchanged working van loads successfully and returns six existing findings.
Desktop refresh and native installed-app verification now pass. Selecting Bed deck
opens bed_lift directly; Max moves the deck from 900 to 1600 mm with mattress and
rails, and a slider gesture moves the inherited assembly together. One Undo restores
the complete gesture while retaining the saved-motion panel; Reset restores 900 mm.
Normal quit/process-exit/relaunch restores the Saved 38-object working van, mm units
and Free 3D view. The saved file digest remains unchanged. Zoom, pan, active tab and
selection are not restored. The subsequent pre-S5 refinement above supplies automatic
framing, selection retention through history and saved-state reconciliation.
Human acceptance remains separate; S5 route editing/corridors/checks are next.


## Viewport projection, gizmos and readability (2026-10-03)

Side (YZ), Front (ZX) and Free views now project positive world Z upward.
Inverse construction-plane/face picking, resize mapping and camera pan/zoom use
matching signs. Orthographic picking supports planes on either side of the view
origin; physical forward-ray intersection is unchanged. Legacy XY drafting keeps
its existing screen convention. The shared viewport ABI and physical document
coordinates are unchanged.

Object, local-handle, point and bounds gizmos use a common app-owned sizing helper:
axis length is 6.5% of the shorter viewport dimension, clamped to 48–100 logical
pixels, with 5–8 pixel endpoint radii and three extra pick pixels. Zoom and object
size no longer enlarge the handles. World orientation axes and drag sensitivity
use the same helper; rotation response no longer shrinks with zoom. Axes retain normal orientation foreshortening.
Visible primitive wireframe edges have an eight-pixel selection tolerance, with
center picking and handle priority retained. Hidden/non-selectable objects are
excluded. Imported mesh edge selection uses a bounding proxy, not triangles.

Measure reference selectors and their menus use full-width rows with measured
text wrapping. Pick in view remains beside the A/B heading. Motion-envelope labels
are separate clipped viewport legend rows showing saved rule ID and current/stale
bounds status. Large scenes can exceed the bounded legend capacity.

Warning-clean build, 497 tests in 47 reported suites, agent-scene-smoke and
illustrative-van-smoke pass. Native review selected a visible edge away from the
center, compared wall gizmos at two zooms, inspected full Measure reference names
and moved the bed from 900 to 1600 mm upward; undo restored its pose. The saved
working file was not overwritten. Desktop package/identity verification is separate
from user acceptance. The later saved-motion continuity refinement above covers repeated findings,
Parts chooser wording and preference persistence. Startup browser I/O remains
before bounded S5 routing.

## Explicit viewport tools and legacy drawing (2026-10-03)

The default viewport tool is Select. View exposes Select/Orbit/Pan buttons; Orbit
and Option-drag enter Free view in 3D, capture input before geometry picking and
work above the old zoom ceiling. Right/middle drag pans instead of implicitly
placing lines. Create → Geometry exposes Draw line and Stop drawing. A first
endpoint is transient; completion creates one undo step. Stop, right click, Escape,
tab changes and choosing another geometry tool cancel the pending line.

The left Delete selected button now handles legacy selected points/lines as well
as the existing scene-object path. Point deletion removes incident lines with
one undo capture. Point picking and the pending marker use fixed pixel sizes.
Hidden controls no longer repaint group chrome over live Create controls.
The shared viewport bridge, pane UI and theme remain reused; schema 19 is unchanged.

Warning-clean build, 491 tests in 47 reported suites and both scene smoke lanes
pass. Native visible Orbit/Pan and canceled first-endpoint checks leave the van
Saved with 38 objects and zero anchors/walls. Option priority is regression-tested.
See [the van navigation guide](illustrative_van_workflow.md#navigation-and-legacy-lines).
The later viewport/readability refinement above resolves projection and Measure
selector/overview label gaps. The newer saved-motion refinement above also covers
repeated findings and session preferences.


## Illustrative van acceptance project (2026-10-03)

A deterministic native example now composes 38 objects, eight assemblies, seven
links, four geometric rules, three saved spatial checks and two motion envelopes.
The model is explicitly illustrative, using meters/right-handed Z-up with a
separate user working copy. It exercises bed assembly lift, nested door hinge,
maintained height/angle, reference geometry, walkway/service space and deliberate
obstruction checks. Generation refuses existing destinations; generic agent
request fields and schema 19 are unchanged.

**View → Fit scene** now frames visible geometry, and physical zoom permits useful
navigation with small engineering grid steps. It changes no geometry or history.
Native regeneration/persistence/exercise checks and mouse navigation regression
pass. See [van walkthrough and audit](illustrative_van_workflow.md) for expected
findings, repairs and limitations. Installed mouse interaction verified Fit, mm
display, bed/door assembly motion with undo/save, links/filtering and read-only
full-range findings. 491 tests / 47 reported suites and both smoke lanes pass.
User acceptance remains separate. The open Saved project lives under Main Edit
Application Support/projects/van_workflow, outside the replaceable runtime.

The subsequent viewport refinement above resolves increasing-Z-downward projection,
Measure selector clipping and overview label overlap. The newer saved-motion
refinement covers repeated findings and relaunch preferences. See the walkthrough
for the protected-folder startup stall and safe app-owned project location. Next:
desktop handoff and startup browser I/O, then
bounded S5 route editing/corridors/checks.

## S4 interval coverage and full-range inspection (2026-10-03)

Measure now has **Moves: B only / assembly** below the existing movement controls.
An explicit containing assembly moves rigidly with B, including nested members and
assembly frames; envelopes sample the whole saved group. Parts → Volumes retains
regeneration, status, size/range, stable IDs and movement/check navigation.

Parts → Checks adds **Inspect motion** for current primitive envelope results.
A read-only orange-wire preview opens at the first sampled failure or closest
sample, with Previous/Next/Close controls and visible position/member feedback.
The visible box is now labeled overview bounds. Checks use a transient union of
member intervals, reducing false obstructions in gaps and door-sweep corners.
Separated saved checks can pass with an explicitly conservative gap; overlap is
still only a warning. Reports expose geometryMethod for interval bounds.

**Check full range** adds bounded adaptive interval separation/contact inspection.
It reports a separated range, contact/clearance failure at a tested pose, or an
unresolved interval. The preview jumps to refined findings without changing the
scene. Budget/depth exhaustion never passes. Authored-input drift invalidates
previews. Downstream followers cannot be frozen as static obstacles.

Schema 19 persists explicit motion scope and relative member snapshots; schema 18
B-only envelopes migrate with retained freshness. The group supports up to 64
design panels/prisms. A must be outside it, and other geometric rules mentioning
members, unsupported kinds, locks and bounds conflicts are refused atomically.
Generation, scope changes and motion retain undo/redo and identity. Reserved
boxes/provenance remain in the authoritative runtime snapshot, outside solids.

See [motion envelopes and previews](motion_envelopes.md) for controls, migrations,
proof and exclusions. Source/mouse tests and native fixtures are separate from
user hands-on acceptance. 487 tests in 47 reported suites pass, including 22 motion
tests; native separated/contact/unresolved fixtures were reviewed. Next is a bounded
van workflow audit using illustrative assemblies, service spaces and movement,
then S5 routing corridors/paths. Independent follower/mesh/multiple-joint motion is deferred.
The dated milestone accounts below retain their original proof/next-step history.

Last updated: 2026-10-04

## S3B/S3C reserved volumes and spatial checks (2026-09-30)

**Parts → Volumes** now creates/edits named Keep-out and Service wire boxes, with
physical dimensions, expandable world position/owner fields and confirmed deletion.
**Parts → Checks** runs read-only obstruction checks and expands saved pair-rule
editing separately. Results expose IDs, names, measured/required gaps, errors or
warnings, object selection and stale-result feedback. Hidden design objects remain
in checks; automatic checks exempt Reference geometry and a volume's declared owner.

Schema 17 persists roles, owner IDs and up to 32 stable intersection/clearance rules.
Existing object identities, assembly movement, undo, old-document migration and
atomic load/edit refusal remain intact. Canonical/runtime snapshots preserve all
spatial records; reserved space is omitted from solid export and camera framing.
Prisms/panels use oriented geometry; mesh bounds are explicitly approximate warnings.
Contact counts as intersection. No structural, triangle-collision or swept-motion
acceptance is claimed. See [controls, contract and limits](spatial_checks.md).

465 tests / 46 reported suites pass, including 8 spatial test cases; producer
smoke, read-only CLI report and package self-test are separate evidence. Native UI
renders were inspected. User acceptance remains pending. Next is S4 sampled motion
and conservative envelopes, using these volume and checker contracts.

## S3a attachment, support and containment links (2026-09-29)

**Parts → Links** now provides source/type/target selectors, Create/Update/New
and confirmed Remove controls, and clickable incoming/outgoing link inspection.
Objects and assemblies use their existing stable IDs; rename/reparent/movement
preserve references. Links describe physical relationships separately from transform
parenting and driving constraints, without changing geometry.

Schema 16 saves up to 128 directed links. Duplicates, self-links, missing endpoints,
invalid types and containment cycles are refused before publish; linked endpoints
must have their links removed before deletion. Undo, old-schema migration, saved
form refresh and canonical/runtime graph preservation are covered.

457 tests / 45 reported suites pass, plus scene-producer smoke and Main Edit package
self-test. Native form and chooser captures were inspected. Human acceptance of
Links remains pending. See [relationship controls and contract](relationships.md).
Next is S3b reserved/service volumes, then S3c intersection/clearance checks. No
physical containment, attachment-fit or structural-capacity checks are claimed.

## S2 semantic organization and rigid assemblies (2026-09-29)

The new **Parts** tab provides Objects, Assemblies and Filters with explicit
selectors, inset fields and action buttons. Names/types, Design/Reference role,
typed properties and assembly parents are saved independently of stable IDs.
Nested assemblies move/rotate through a compact numerical form; reparent preserves
world pose. Conflicting locks, bounds, rules or history failure refuse the whole edit.

Schema 15 preserves the semantic records and rigid assembly frames. Existing
world geometry remains authoritative, with parent-local frames derived from it.
View filters affect rendering, selection/list and annotations while leaving saved
geometry, validation and exports intact. Canonical/runtime exports preserve the
semantic extensions and assembly snapshot. Mesh world rotation composition was
corrected after a nested-group regression exposed the old Euler-addition defect.

451 host tests / 44 reported suites pass, including 9 Engineering tests and 33
Constraints tests; producer smoke, shape tool build and Main Edit package self-test
pass. Native rendered forms were inspected. Human acceptance of Parts remains
pending. See [S2 workflows, schema and limits](semantic_assemblies.md).
Next is S3 explicit relationships, reserved/service volumes and structured checks;
new engineering primitives and sampled envelopes remain later bounded work.

## S1E bounded hinges and S1 refinement (2026-09-29)

Bounded movement now includes **Travel** and **Hinge**, using the same compact
Measure form: Min, Max, Position, an always-visible slider, and Min/Max/Reset.
Hinges join authored pivots and rotate in an explicit world plane; their scalars
are degrees independent of length display units. Schema 14 preserves the captured
hinge frame and reset angle. Existing linear travel and fixed rules remain supported.

The S1 refinement adds named Top/Side/Front/Free views, an Advanced physical grid-step
field, direct access to pivot offsets, form refresh after undo/redo or rule changes,
and viewport range/actual-target feedback while Measure is open. Primitive scene-list
positions/sizes and the object selection header display physical units. No second geometry store was introduced.

442 host tests / 43 reported suites pass, including 33 Constraints tests. Producer
smoke, shape-sanity tool build and Main Edit package self-test pass. The final native-rendered
hinge fixture was inspected, and a disposable native app loaded the saved fixture
through its catalog. Further native coordinate clicks failed in the computer-control
tool with `noWindowsAvailable` while the app remained running; the complete native
Measure mouse walkthrough remains unverified. SDL mouse-event regressions cover the
controls, slider gesture history and invalid input. These are distinct proof levels.

The user confirmed the earlier compact Measure/linear-travel redesign works. That
acceptance does not cover this new hinge. [S1 audit and next boundary](s1_engineering_audit.md)
records the implemented primitive scope, remaining CAD gaps and S2 continuation.

## Measure workflow redesign (2026-09-29)

The Measure tab now shows one selected tool at a time. Buttons, inset numeric fields,
plain readouts and dropdowns have distinct styling. Advanced references/offsets and
saved-constraint management are collapsed initially. Travel always displays its slider
and Min/Max/Reset controls, disabled before creation; invalid limits receive local feedback.
Staged limit changes disable movement until applied or reset. Unit selection converts
staged lengths without changing physical geometry. Backend constraints and schema are unchanged.

437 host tests / 43 reported suites pass (28 Constraints tests), plus Main Edit package
self-test. Native-rendered initial, error and active travel fixtures were inspected.
At this checkpoint human usability review was pending. The later user-confirmed
compact flow and S1E continuation are recorded above. Automated tests alone do not
establish human acceptance.

## S1e bounded linear travel (2026-09-28)

Measure now includes Travel with explicit Min, Max and Position fields, a live
slider, and Min/Max/Reset buttons. Rules retain a world-fixed rail, transverse
separation and moving-object orientation; downstream constraints follow accepted
motion. Direct on-rail translation updates the saved position. Sideways/rotational,
out-of-range and downstream conflicting edits are refused atomically. A slider
gesture owns one undo step. Reset, undo/redo and schema-13 save/reopen are covered.
The nested schema lookup was also corrected so schema-12+ offsets are required.

436 host tests / 43 reported suites pass, including 27 Constraints tests. Producer
smoke, shape-tool build and Main Edit package self-test pass. A disposable bed-lift
render with 900–1800 mm travel and an attached sensor was inspected; these are
provisional demonstration values, not van measurements. Human interaction review
remains separate from automated coverage. [Controls and limits](geometric_constraints.md).
This historical slice is superseded by the Measure review gate above.

## Persistent Measure pane (2026-09-28)

The shortcut-heavy modal is replaced by a compact right-hand **Measure** tab.
Object/feature selectors, Pick in view, numeric fields, Distance/Join/Angle actions,
expandable pivot offsets, and a saved-rule selector are visible mouse controls.
Measurement selections persist across tab switches. The pane scrolls independently;
only focused text entry captures typing. Enter completes target entry without
editing geometry; Move once/Save rule/Update rule explicitly apply it. Existing
scene/model tabs and geometry contracts are preserved. Tab widths use their actual
labels so adding Measure does not unnecessarily expand the right pane.

432 tests / 43 reported suites pass, including two complete mouse-event UI
regressions. Native compact-pane render inspected; Main Edit package self-test
passes. [Controls and limits](geometric_measurements.md). Next is user review of
this editing workflow before S1e travel and motion-envelope implementation.

## S1d offset pivots and viewport feedback (2026-09-28)

References can carry physical U/V/N offsets, rotate with their owning primitive,
and serve as off-center fixed-mate pivots. Measure X/Y/Z stages offsets; R/O/M
saves them atomically with a rule. Schema 12 preserves offsets, reads zero-offset
schema 11 rules, and rejects malformed or unsatisfied snapshots.

The selected object's related rules show live actual/target status and pivot/direction
markers in the normal viewport. Feedback uses the solver's own tolerances and is
read-only. Hidden participants are excluded. The offset edge-mate render was inspected.
430 tests / 43 reported suites pass, including 21 constraint tests. Producer smoke
and shape-tool build pass. The isolated Main Edit package self-test passes. See [the contract](geometric_constraints.md) for controls,
proof and limits. Next: S1e bounded travel, then pose sampling and envelopes.
Reusable named datum entities are not part of the embedded-offset implementation.

## Persistent constraints and unified mutations (2026-09-28)

S1c now ships: saved signed projected distances, stable rule/entity references,
deterministic driver chains and atomic conflict rejection. Numeric edits, primitive
move/resize/rotate/scale APIs and their drag/dialog routes share the transaction.
Accepted constrained drags reserve one undo snapshot; deletion/cut replacement
refuses participating objects. Schema 11 validates rules on load/save/export.
Initial S1d adds coincident points and fixed planar relative-angle mates.

425 tests / 43 reported suites pass, including 16 constraint tests and actual mouse
input, graph/lock failures, history reservation, persistence/export and UI lifecycle.
Agent-scene producer smoke, the separate 25-test resize suite, shape-tool build
and isolated Main Edit package self-test pass. Saved 20 mm and 30-degree UI fixtures rendered
and inspected. [Contract, controls and remaining limits](geometric_constraints.md).
Earlier entries below describe their respective checkpoints.

## Exact reference placement (2026-09-28)

Measure now offers D to set a signed projected distance and C to align reference
points. A stays fixed; B translates once. Unit-suffixed entry, exact candidate
validation, atomic refusal, single-step undo, reopen and runtime export are covered.
These are S1c prerequisites, not persistent driving constraints. Full tests pass:
409 across 42 reported suites. The placement entry fixture was rendered and inspected.
See [contract and next mutation-boundary work](geometric_measurements.md).

## Viewport measurement picking (2026-09-28)

Measure now offers K / Pick mode with selectable origin/axis/face-center markers,
A/B annotations, a guide line and physical distance readout. Hidden/unselectable
objects are excluded. Marker picks are explicit X-ray datums, not occluded surface
queries. Keys 1/2/3 select world XY/YZ/XZ orthographic views without changing the
construction plane or geometry. Failed picks preserve the selected reference.
Full tests: 404 across 41 reported suites. Source visual fixture was rendered and
inspected; Main Edit package self-test passed. The S1c and initial S1d follow-up
that was next at this checkpoint is now implemented above.

## Geometric measurements (2026-09-28)

The Object tab now includes a read-only Measure dialog for primitive origins,
U/V/N axes and named faces. It reports point distance, explicit-axis signed
projection, unsigned direction angle, signed planar angle and parallel-plane gap.
Stable entity/feature operands resolve current geometry; invalid or deleted
references return explicit status. No saved annotations or driving constraints.
See [geometric measurements](geometric_measurements.md) for exact definitions,
usage, proof and limitations. Full source tests: 403 across 41 reported suites;
Main Edit package self-test passed, with a bounded candidate UI angle check.

## Spatial Engineering Audit (2026-09-28)

The S0 physical-document foundation and S1a numerical-edit slice are implemented
at VERSION `0.4.0`. Layout schema 10 preserves physical scale, explicit
right-handed Z-up context, persistent string IDs and object flags. Versions 0–9
remain readable. Imported world_scale now survives re-export and UI conversion.
Primitive dimension and position dialogs share a candidate/undo command boundary;
dimension inputs accept unit suffixes. Bounds and plane-lock conflicts reject the
edit without mutating geometry or history.

See [engineering document contract](engineering_document_contract.md) for scope,
tolerances, migration and verification. Full tests, resize tests, scene producer
smoke and Main Edit package self-test pass. Editor first-frame capture succeeded;
interactive dialog acceptance and physical measurement accuracy are not claimed.
The reference/measurement portion of S1b is now implemented as described above;
persistent distance/angle constraints remain next.
Semantic views, nested assemblies, motion and full transactional agent APIs
remain future work in [spatial engineering direction](spatial_engineering_direction.md).

## Managed Vulkan Presentation Baseline

- The default vendored shared subtree now carries canonical shared commit
  `8814728240febb552f6bb7a9fd789b979dafab2e`, including `vk_runtime 0.6.0`
  beneath `vk_renderer 1.3.1`.
- LineDrawing keeps its existing Vulkan adapter and editor/render ownership;
  the renderer delegates instance/device/queue lifecycle to the runtime while
  preserving the compatibility handles used by the app.
- `make vulkan-rollout-contract` independently hashes every tracked canonical
  Vulkan runtime/renderer source file against the managed copy.
- `make vulkan-rollout-self-test` requires validation-clean startup, real
  resize, shutdown/restart, shared runtime/device handle identity, nontrivial
  capture readback, and high-DPI drawable extents. The Apple M2 proof passes at
  2.0x scale from `2560x1440` to `2880x1800`, with zero validation warnings or
  errors.
- This is presentation lifecycle adoption only. Scene authoring, CPU mesh
  preview/rasterization, editor policy, and application semantics remain
  LineDrawing-owned; no Vulkan compute/residency/timing workload API is used.
- The presentation adoption above was recorded at version `0.3.0`; the
  current source VERSION is `0.4.0`. This historical presentation proof does
  not establish a new release, Registry promotion, Linux application proof,
  or RayTracing rollout.

## Program Identity
- Repository directory: `line_drawing/`
- Public product name: `Sculpt`
- Internal/repo/runtime identifiers still use `line_drawing` and `LineDrawing`
  in launcher, log, binary, and source-level contracts where required
- Primary runtime entry:
  - `src/main.c` -> `line_drawing_app_main(...)`
  - wrapper shell: `include/line_drawing/line_drawing_app_main.h`, `src/app/line_drawing_app_main.c`

## Persistent Main Edit Development Identity

- Canonical source remains `main`; functional integration uses the persistent
  `codex/line-drawing-main-edit` lane documented in
  `docs/main_edit_worktree.md`.
- The isolated development package is `sCulpt Main Edit.app`, bundle
  `com.cosm.sculpt.main-edit`, with separate `LineDrawing-Main-Edit` runtime
  and log namespaces.
- The required `package-desktop-main-edit`,
  `package-desktop-main-edit-self-test`, and
  `package-desktop-main-edit-refresh` targets are present.
- Main Edit packages embed generic exact source/binary identity and guard
  against source mutation during packaging, canonical Desktop overwrite, and
  replacement of a running development app.
- This local development identity does not change `VERSION`, canonical
  `sCulpt.app`, release history, Registry state, or publication authority.

## Current Shipped State
- Default launch is now menu-first:
  - `src/main.c` owns a host-level `MENU` vs `EDITOR` runtime split
  - the top-level host menu is implemented under `src/Menu/`
  - the editor/runtime session remains the existing authoring workspace behind that host seam
  - the current Phase 2 slice gives the host menu explicit navigation, filter, content, detail, and footer regions instead of a flat action card
  - catalog rows and the detail pane now render lightweight cached wireframe previews plus preview-derived primitive and bounds metadata
  - the host menu now also has a dedicated browse section with header-level root-picker controls plus nearby scene-like directory suggestions around the current input root
  - the host menu now also has a dedicated recents section for reopening recent layouts/scenes and switching back to recent input/output roots
  - `Esc` in the editor now returns to the host menu instead of closing the program directly; `Esc` from the host menu still quits
  - the host shell now uses stronger pane framing, section-count badges, selected-state accent bars, and better separated preview/detail blocks so the top-level surface reads more like a durable tool shell
  - the host list lane now renders a visible draggable scrollbar for recents, layouts, scenes, and browse when content exceeds the viewport, and the top-level host also accepts `Ctrl/Cmd + B` for the native input-root picker (`Shift` variant for output root)
- 2D/3D parity lane is complete (`LD-U0` through `LD-U6.6`).
- Trio scene-authoring and deep 3D behavior foundation lanes are complete through `LD3D-F8`.
- Primitive authoring contract is active for planes and rectangular prisms with typed object payloads.
- Scene-editor display is now an explicit four-mode contract:
  - `Bounds` renders only transform/bounds cages;
  - `Wire` renders the real mesh preview feature edges plus a transparent,
    view-dependent coherent-LOD outline so smooth complex meshes retain a
    readable perimeter;
  - `Solid` renders an opaque, depth-tested runtime-mesh surface through
    coherent interactive/settled LODs;
  - `Material` uses the same real surface path with app-local material tinting.
- Complex-mesh quality state is separated from projection and interaction
  feedback: zoom, pan, viewport resize, and plane-offset changes reraster the
  established LOD without demotion; orbit direction and mesh-transform changes
  may briefly use an 8,000-triangle/60%-scale tier before returning to the
  18,000-triangle/75%-scale settled tier. Appearance changes retain the current
  tier, and hover/selection draw later without recoloring or demotion.
- Shared `core_mesh_preview` `0.5.0` now owns the renderer-neutral coherent
  indexed LOD builder used by complex-mesh editor previews. LineDrawing keeps
  only its viewport quality policy, CPU depth rasterization, outline
  composition, texture cache, and interaction overlays app-local.
- Legacy saved scenes that still reference `Desktop/<mesh-library>` recover the
  known `Desktop/stls/<mesh-library>` relocation at read time. The scene file
  is not silently rewritten, and shared canonical mesh data remains unchanged.
- Dense-scene object selection baseline is now improved:
  - plain object-body hover/click resolves to the nearest projected object
    origin instead of whichever overlapping object bounds happen to win the
    generic body hitbox ranking
  - direct object handles/gizmos, anchor lanes, and scene-bounds lanes still
    keep higher targeting priority than generic object-body picks
  - selected and hovered authored objects now show an explicit origin marker in
    the viewport
- The editor shell modernization lane is now complete through Phase 6:
  - the old always-open grouped-control sidebars are now routed through a
    tabbed shell
  - left editor tabs are:
    - `Scene`
    - `File`
  - right editor tabs are:
    - `View`
    - `Create`
    - `Object`
  - object workspace remaps the same shell into CAD authoring terms:
    - left `Model`: object-asset status, body/sketch/operation counts, current
      selection, and first-pass body/sketch/operation rows from
      `ObjectAuthoring`; rows now select the matching body, sketch, or
      operation context, and operation history uses a clipped scroll lane with
      wheel/hover/scrollbar handling for long stacks. Its object-mode action
      contract is selection and navigation, not command launching or asset I/O
    - left `Assets`: object asset save/load/new/export actions plus asset
      browser/path controls; it owns file/root actions, not model selection
    - right `Tools`: face/sketch/extrude actions plus a static `Command
      Actions` card and a separate `Active Command` card for current target,
      sketch, selected-operation, and extrude-depth state; armed extrude
      previews now expose `Depth -` / `Depth +` controls that adjust by the
      current grid step. Its action contract is command selection,
      arm/commit, and live command-parameter editing
    - right `Properties`: selected object/entity details for operations,
      sketches, bodies, and selected faces plus dimension, gizmo, and transform
      controls; it owns selected-entity inspection and persistent body/object
      edits, while command tools stay in `Tools`
    - right `View`: viewport navigation, zoom, view-plane, and editing-mode
      controls
    - object-authoring refs now carry stable app-local face ids for the current
      primitive/evaluated face model while preserving primitive face labels as
      the UI compatibility adapter. Object-mode `Model`, `Tools`,
      `Properties`, and the top status overlay now expose those stable
      `FaceID` values beside the compatibility face labels
    - face-ref diagnostics now classify unset, missing-body, missing-face, and
      stale-adapter states. Replay/evaluation fails invalid sketch/extrude
      refs explicitly, and object-mode `Model` / `Properties` rows show the
      face-ref status
    - Phase D editable topology now has its first base: object-authoring
      documents rebuild deterministic evaluated vertex, edge, and face records
      for plane and rectangular-prism bodies; faces bind to corner vertex ids
      and boundary edge ids; edges retain bounded face adjacency; and explicit
      vertex/edge selection refs are available for constrained-gizmo movement.
      The object viewport now draws evaluated vertex/edge overlays in object
      mode, emits topology hitboxes above generic object-body picks, and
      preserves clicked/hovered vertex or edge refs in the authoring document.
      Selected topology vertices/edges now resolve into the existing object
      constrained-gizmo lane, rendering legal local axes at the selected
      vertex or edge midpoint and emitting gizmo-axis hitboxes. Topology gizmo
      drags are non-mutating until the next semantic topology-operation slice,
      because the current evaluated topology is rebuilt from primitive bodies.
      The object-mode `Model` summary reports evaluated body/vertex/edge/face
      counts
    - object center-gizmo work now has first-pass mode parity for scene
      objects: `Move`, `Rotate`, and `Size` render with distinct endpoint
      visuals, and active drags report move distance, rotate angle, or size
      factor through the topbar. `Size` scales the selected object uniformly
      from any center-gizmo axis, while `Shift` applies directional stretch
      along the grabbed axis where the selected primitive supports it. Shift
      also keeps move/rotate drags in the smooth/non-quantized path.
    - selected-object exact dimension controls now support width/height edits
      for both planes and rectangular prisms; exact depth remains
      rectangular-prism-only
    - object-authoring runtime mesh compile now has a first Phase B baseline:
      operation-backed documents evaluate into deterministic runtime mesh
      arrays, current stable face ids become `face_<face_id>` surface groups,
      bounds/triangles/group references validate before success, and saved
      operation-backed assets can write `mesh_asset_runtime_v1` JSON through an
      app-local carrier over the vendored shared
      `CoreMeshAssetRuntimeContract`
    - object-mode now exposes the first Phase B runtime export affordance:
      `Export Mesh` compiles the attached operation-backed authoring document,
      writes a `<asset>.runtime.json` runtime mesh sidecar beside the object
      asset root, and reports export diagnostics plus the last runtime mesh
      path in the object workspace summary
    - imported STL metadata now has a bounded CLI/test harness:
      `make -C line_drawing imported-mesh-harness-smoke` builds
      `src/Tools/imported_mesh_harness.c`, feeds deterministic STL fixtures
      through `third_party/codework_shared` `core_mesh_asset` +
      `core_mesh_compile`, writes `mesh_asset_authoring_v1` imported-mesh
      metadata, writes file-backed `mesh_asset_runtime_v1`, emits a one-object
      `scene_runtime_v1` with a `mesh_asset_instance`, and records an
      `import_summary.json`. The harness currently covers the ASCII
      tetrahedron baseline plus the richer stepped-column fixture
      (`16` vertices, `24` triangles) used by downstream RayTracing visual
      proof. Through vendored `core_mesh_compile` `0.5.0`, the same harness now
      supports bounded ASCII and binary STL import proofs, including direct
      user-file binary imports, while keeping UI import work deferred. This is
      not yet the in-app file picker/import UI; it is the first bounded input
      harness that ties the shared imported-mesh path into LineDrawing.
      The in-app STL file browser now uses the shared file catalog with bounded
      recursive STL discovery, so selecting a curated library root can surface
      nested paths such as `curated/<asset>/source/<asset>.stl` instead of only
      files in the selected directory or one child level.
    - Phase C scene asset instance integration now has a complete app-local
      reusable asset instance baseline:
      scene mode can place the last exported object runtime mesh sidecar as a
      transformable `mesh_asset_instance`, stores the sidecar asset id/path,
      bounds, vertex count, and triangle count in layout JSON schema v9,
      summarizes mesh instances in the Scene/Object panes, renders a bounds-box
      proxy in the viewport, and exports the object as canonical
      `geometry_ref.kind = "mesh_asset"` with LineDrawing mesh-instance
      extension metadata. Scene mode also exposes a `Mesh Assets` file-browser
      mode that scans `.runtime.json` sidecars under the object asset root
      and places the selected sidecar through that same transformable instance
      path. Switching a selected mesh asset instance into object mode now
      derives the source object asset path from `<asset>.runtime.json`,
      reloads `<asset>.json`, and attaches its authoring document when present.
      Returning to scene mode refreshes every scene mesh instance that shares
      the source runtime sidecar, preserving each scene object id and transform
      while updating asset id, bounds, vertex count, and triangle count
      metadata. Full mesh viewport rendering, material overrides, richer
      imported-mesh picker UI, and deeper asset-library management remain
      Phase D-adjacent follow-up work rather than blockers for the Phase C
      reusable-instance lifecycle.
  - pane targets are now rebalanced:
    - the left side remains wider for the scene list and file controls
    - the right side is reduced more aggressively through the pane-host target
      path so the viewport keeps more room without collapsing the object tab
  - existing actions are still the same underneath, but they now sit behind
    stable dedicated shell surfaces instead of one flat always-open stack
  - the left `Scene` tab is now a real scrollable scene object list:
    - the pane now starts with a dedicated `Scene / Selection` summary card
    - the summary shows total object count, plane/prism split, and current
      selected-object context including size and lock state
    - the summary also exposes anchor/wall counts so the pane reflects the
      broader scene graph
    - authored plane/prism objects render in stable row order
    - rows now show object id, primitive kind, position, size, and lock
      metadata in a denser three-line presentation
    - the scene list now behaves like a selection-first object browser:
      - single-click selects a row without changing the expanded layout state
      - double-click on the same row toggles it open or closed
      - expanded rows surface rotation, scale, and frame-origin context
    - row hover and row-click selection stay in sync with editor object
      selection state
    - the scene list now exposes a visible draggable scrollbar instead of
      relying on wheel-only scrolling for longer object sets
    - the scene browser now renders through an explicit clipped viewport:
      - expanded rows stay inside the list surface instead of bleeding into
        selection or bounds controls
      - the scrollbar is slimmer and separated from row content by a dedicated
        gutter instead of reading like part of the row lane
    - the pane now includes scene-local `Clear Select` and `Delete Obj`
      actions below the list
    - scene-list hit routing is clipped to the real list surface so the lower
      scene controls are not swallowed by generic list clicks
    - scene-pane layout now uses explicit owned section rects for:
      - summary
      - scene browser
      - selection actions
      - bounds controls
      instead of deriving the browser height from later button bounds
    - scene-bounds controls now live at the bottom of the same left `Scene`
      lane instead of in a separate right-side scene tab
  - the right `Object` tab now has a dedicated selected-object inspector:
    - a reserved object-context card renders at the top of the tab
    - selected object id, kind, dimensions, position, rotation, and lock state
      are visible without reading a footer summary
    - object-local edit controls remain directly underneath that inspector
  - the right `View` tab now has a dedicated live state card:
    - space mode, zoom/grid state, construction-plane context, delete mode,
      and current selection state are visible above the view controls
  - the right `Create` tab now has a dedicated live state card:
    - active plane, grid step, display-unit-aware primitive preview sizing, and
      primitive readiness are visible above the create controls
  - the left `File` tab now has a dedicated `File / Session` summary card:
    - layout, scene, input root, output root, and dirty/clean session state are
      visible above the file/root buttons
    - the file lane now also has an explicit owned browser section below the
      file/session-path controls instead of relying on a transient popup overlay
    - `Load JSON` and `Load Scene` now switch that browser section into
      persistent JSON/scene modes
    - the browser now stays active across normal file-pane actions instead of
      disappearing as soon as another panel action runs
    - browser content now renders with:
      - mode title
      - current input-root path
      - clipped entry list
      - active-row highlight
      - slimmer scrollbar lane
      - helper footer copy
    - clicking a JSON/scene row now loads that entry while keeping the browser
      active
    - file-browser mode now persists in ignored runtime state so the pane can
      restore whether it was last in JSON mode or scene mode
    - startup now follows that persisted file-browser mode:
      - JSON mode reopens the last loaded layout when available
      - Scene mode reopens the last loaded scene when available
    - the host-menu catalog and the editor-local file browser now share one
      app-local discovery helper for JSON layouts plus authored-scene
      directories, which narrows the earlier risk of the two surfaces drifting
      apart on what counts as a loadable scene
    - recent-context history now seeds the last-known layout/scene startup
      paths before the restore pass runs
    - after startup restore, the file browser rebuilds from the real active
      session state so the active row highlight comes from the loaded file
      rather than a UI-only remembered selection
    - the file browser now also persists a remembered last JSON entry and last
      scene entry for cases where no active loaded session currently matches
      the browser root
    - the file browser now also persists separate JSON and scene browser roots
      instead of relying on one shared browsing root for both modes
    - browser rebuilds resolve the active row by preferring:
      - the real active loaded layout/scene path for the current mode
      - otherwise the remembered last JSON/scene entry for that mode
    - remembered-entry persistence now runs through the shared layout/scene
      load path instead of only browser-row clicks, so direct loads and startup
      restore keep the browser highlight coherent
    - direct layout/scene loads now refresh the browser immediately, so row
      highlighting updates as soon as session state changes instead of waiting
      for the next browser-only interaction
    - `Load JSON` and `Load Scene` now split behavior by click depth:
      - single-click switches the lower browser mode and uses the persisted
        root for that mode
      - double-click opens the corresponding mode-specific folder picker and
        updates that mode’s saved browser root
    - plain browser-mode switching no longer rewrites the live session root
      just to repopulate the lower browser
    - actual loads from the lower browser still re-establish the live input
      root from the active browser root before loading
    - if the input root changes and neither the active loaded session nor the
      remembered entry exists under that root, the browser now degrades to no
      highlighted row rather than implying stale selection state
    - the `File / Session` summary now includes an explicit browser-status line
      so the pane tells the user whether the current browser state reflects an
      active session row, a remembered row, a mode with no matching row, or a
      mode with no entries
    - the same summary card now also renders an explicit action-hint line, and
      the browser footer now reuses that same state-aware hint text so the
      user can tell when:
        - `Use Session` is just re-centering a true active row
        - `Use Session` is restoring the live session away from a remembered
          fallback row
        - `Clear Last` is only relevant because the current row is remembered
    - the browser-state explanation now runs through one shared helper
      contract, so the summary card, browser footer, and regression tests all
      describe the same active-versus-remembered semantics
    - the browser list now also marks those two row types visually:
      - true active-session rows render with a `LIVE` chip
      - remembered fallback rows render with a distinct `LAST` chip plus a
        warmer row accent
      - unmatched roots stay chip-free, so the file pane no longer relies on
        text alone to distinguish remembered fallback from the real live row
    - the generic root controls are now explicitly labeled as `Session Paths`
      so they no longer imply that they edit the same root as the lower
      JSON/scene browser
    - `Session In Edit` / `Session In Pick` now target only the live session
      input root; mode-specific JSON/scene roots remain owned by the
      corresponding `Load JSON` / `Load Scene` double-click picker flow
    - the file summary now shows `Browse In` separately from `Session In` so
      the current mode-specific browser root is visible independently from the
      live session input root
    - session input-root edits now skip unnecessary browser rebuilds when the
      active JSON/scene browser is pinned to a different saved mode root, which
      keeps the two control lanes from fighting for ownership
    - the file pane now has a stronger structural layout:
      - the top `File / Session` card is denser and shorter
      - the persistent JSON/scene browser now owns the elastic middle section
      - `File / IO` and `Session Paths` are anchored at the bottom instead of
        consuming the browser’s height first
      - those bottom file-pane control groups now use compact 2-column button
        rows where appropriate instead of always using full-width stacked
        buttons
    - the file-tab browser no longer blocks left-pane resizing:
      - splitter drag now gets first chance before the persistent file browser
        captures a click, so the left pane can still be resized while the
        `File` tab is active
    - browser refresh now scrolls the active/remembered row into view instead
      of always resetting long lists to the top
    - `File / IO` now also exposes compact browser quick actions:
      - `Use Session` retargets the browser to the best current working-session
        row for the active mode:
        - the current loaded layout/scene path when it still exists
        - otherwise the most recent layout/scene path from recent-context
          history
      - session input-root edits now preserve that real loaded layout identity
        instead of rewriting it back to the new root's default
        `layout_config.json`, so `Use Session` can recover a true active row
        after ordinary session-root changes
      - `Clear Last` removes the remembered fallback entry for the active
        browser mode so the pane can degrade cleanly to no highlighted row
        when no real active-session match exists
    - the browser header now shows mode plus entry count instead of repeating
      the current browse root above the list; the root remains visible in the
      summary card instead
    - file summary lines, browser rows/footer, and compact file-pane button
      labels now clip to their pane/view bounds instead of forcing `...`
      substitution, so pane resizing behaves more like narrowing a viewport
    - active file-browser rows and active file/scene tab fills now use subtler
      blended fills so bright presets keep readable contrast
    - invalid or empty candidate roots are rejected before mutating the
      current input root, so a failed folder pick does not clobber the prior
      working root
    - `Export Runtime` now writes scene-package directories under the configured output
      root and uses the active layout/scene path only as a naming hint; it no
      longer silently overwrites the active authoring directory just because a
      scene is loaded
    - successful runtime export records package/authoring/runtime/dependency/receipt
      paths and the bundle digest for diagnostics but leaves the active/recent source
      identity unchanged; failed exports never expose an incomplete final
      directory
    - export feedback is visible but temporary: the File summary and
      `Export Runtime` button briefly shows success/failure state, then returns to
      the normal action label
    - strict authored-versus-compiled scene truth remains explicit:
      `scene_runtime.json` is still compiled output only, and the regression
      suite now directly checks that import rejects it as a load source
    - document lifecycle is now explicit in current `main`: `Save` updates the
      active layout/object/scene-authoring source, Scene Save As creates a
      collision-safe sibling package by allowlisting authoring-owned assets and
      attachments without copying compiled runtime/dependency/receipt output,
      and Export Runtime compiles derived output without stealing the active
      source identity or clearing its dirty state
    - the File summary identifies the active source kind/path and reports the
      sibling runtime as missing/current/stale/unknown. Exported bundles now
      carry a `sculpt_scene_package_v1` entrypoint plus digest-bound compiler
      provenance in `scene_export_receipt_v1`; the receipt binds the package
      entrypoint as well as authoring/runtime/dependency payloads;
      the adjacent source-session freshness label remains a filesystem
      timestamp diagnostic until receipt verification is attached there
    - the unattended scene-pipeline and agent-scene smoke lanes now resolve
      their tool binaries through the current Makefile path contract instead of
      assuming the older flat `build/bin/` layout
  - the editor-local JSON/scene browser is now owned by the left `File` lane
    geometry instead of floating as a transient lower overlay that disappears
    after unrelated actions
  - the live runtime frame renderer now routes through the unified
    `Render_UIPanel(...)` pass, so scene/view/create/object summary surfaces
    are actually drawn in the packaged app instead of only existing in the
    panel modules
  - the final shell polish pass is now in:
    - pane surfaces have stronger framing instead of reading like flat
      edge-attached debug columns
    - active tabs and hovered buttons now carry clearer accent treatment
    - group sections now use clearer title chips
    - the left scene list now renders inside a framed owned surface with
      stronger selected/hover row accents
    - file, view, create, and object summary cards now use top accent bands
      plus internal divider lines
  - the pane language is now more internally consistent:
    - top summary cards use denser stable copy instead of taller debug-style
      phrasing
    - middle workspace/inspector surfaces use a slightly darker shared fill so
      they read as intentional working layers instead of the same flat pane
      background
    - summary cards, list rows, browser rows, and right-pane lower controls
      now route through shared clipping/layout/visual helpers instead of
      ad hoc per-pane overflow rules
  - future editor UI work can now return to smaller usage-driven follow-ups
    instead of continuing this structural shell migration
- The editor topbar is now the production top-level menu/status lane rather
  than an overlay diagnostic string:
  - one renderer owns the top-pane surface and avoids stacked duplicate text
    overlays
  - it shows workspace mode, clipped selected-object context, file/dirty state,
    mode, view, plane, construction-plane readout, bounds state, gizmo mode,
    live operation, and undo/redo controls
  - `Mode`, `View`, `Plane`, `Bounds`, `Gizmo`, `Undo`, and `Redo` are
    clickable status chips wired to the same backend actions as their keyboard
    shortcuts
  - `CP` remains a readout until construction-plane picker/stepper behavior is
    deliberately designed
  - active center-gizmo drags update a separate `Op` chip and the primary
    selection line with move, rotate, or size operation reports
- Agent-authored room-review scenes now have an optional deterministic
  refinement lane through `line_drawing/tools/agent_scene_refine.py` for:
  - opposite-corner default camera placement in open corner rooms
  - authored camera-path points placed outside the floor footprint by default
  - camera-path edits no longer forced back inside authored scene bounds
    because the refiner disables `bounds.clamp_on_edit` on refined requests
  - authored camera yaw/pitch now match focus-target direction so app-side
    camera vectors and startup previews agree with headless runtime sampling
  - transparent tall-prism spacing cleanup
  - sampled RayTracing light-path clearance around object clusters
  - first-class RayTracing lighting intent synthesis through
    `extensions.ray_tracing.authoring.lighting_policy`
  - front-biased generated review modes now start camera-side of the subject
    by default and keep control points inside the requested front hemisphere
    unless a full orbit, rim/backlight path, or `allow_backlight` is explicit
  - supported generated-lighting intents include front key orbit, front
    corkscrew, front vertical sweep, full/fixed-height object orbit, high
    shadow orbit, rim light, and transparent-review lighting
  - `extensions.ray_tracing.authoring.ambient_policy` now expands into the
    RayTracing bridge's existing `authoring.environment` fill-light settings
    for none/review-fill/transparent-fill/studio modes
- `agent_scene_tool` now accepts first-class `mesh_asset_instance` request
  objects for RayTracing-facing mesh probes:
  - the request supplies a stable authored object id, `asset_id`,
    request-relative `asset_source_path`, transform, variant, and optional
    canonical `material_id`
  - the tool skips mesh instances during Layout primitive-store creation, then
    injects them into `scene_authoring.json` as full-3D
    `object_type = mesh_asset_instance` objects with
    `geometry_ref.kind = mesh_asset`
  - referenced runtime mesh sidecars are copied into both
    `<out>/assets/mesh_assets/` and the app-loadable
    `<run_dir>/line_drawing_app_load/assets/mesh_assets/` folder
  - the low-poly sphere fixture renders through RayTracing headless as 48 mesh
    triangles plus a 2-triangle floor, proving the request schema, canonical
    compile, asset sidecar lookup, and BVH render path are connected
  - the same schema has been used for higher-fidelity moving-light mesh-sphere
    worker proofs; the authored scene remains LineDrawing-owned while
    RayTracing owns runtime mesh loading, native `3D` triangle build, BVH
    traversal, shading, and publication
- Scene export/compile path is wired and deterministic for canonical scene contract fixtures, and the desktop UI exports full scenes as stable per-scene directories through the configured output root.
- The current scene-directory export contract is:
  - derive a scene stem from the current layout filename
  - render canonical authoring JSON in memory
  - compile deterministic runtime JSON through shared `core_scene_compile 0.8.0`
  - discover and hash validated file-backed runtime-mesh dependencies in Sculpt
  - canonically sort/digest `scene_dependencies.json` through shared code
  - retain each runtime-mesh payload at the content-addressed canonical path
    `dependencies/mesh_asset_runtime/<sha256>` inside the same transaction
  - durably stage `scene_package.json`, authoring, runtime, dependencies, and `scene_export_receipt.json`
  - verify artifact and payload bytes, counts, provenance, compiler
    compatibility, receipt identity, and the expected bundle digest before
    reporting export success
  - publish `<output-root>/<scene-stem>/` with one atomic no-replace rename
  - refuse an existing destination without modifying the prior iteration
  - preserve the resulting paths and bundle digest for UI diagnostics/logging
- Exported bundle paths and digests are derived-output evidence, not editor
  document identity. The loaded source remains active until an explicit Load
  or Save As.
- The scene-project export integration now has an explicit tool/API entrypoint
  that writes canonical `scene_authoring.json`, compiled `scene_runtime.json`,
  `scene_project.json`, `object_manifest.json`, and empty downstream scaffold
  directories into one selected project root while preserving ordinary scene
  export behavior. Project metadata uses root-relative paths, and
  `scene_authoring.json` remains the editable LineDrawing import target;
  `scene_runtime.json` remains compiled output only.
- Scene-project export now populates `object_manifest.json` for live
  `mesh_asset_instance` objects that reference runtime mesh sidecars. The
  manifest records stable object ids, display names, object kind, mesh/source
  asset ids, vertex/triangle counts, extension presence flags, and
  project-relative `assets/mesh_assets/*.runtime.json` sidecar paths. Referenced
  runtime mesh sidecars are copied into the project `assets/mesh_assets/`
  folder during export, while `scene_authoring.json` remains the editable import
  target and `scene_runtime.json` remains compiled output only.
- The compiler-units rollout now has an initial authoring/export seam:
  - explicit toolchain commands:
    - `make -C line_drawing toolchain-contract`
    - `make -C line_drawing dump-sema-canonical-scene-export`
    - `make -C line_drawing dump-sema-canonical-scene-export-primitives`
    - `make -C line_drawing dump-sema-scene-import`
    - `make -C line_drawing clang-build`
    - `make -C line_drawing fisics-build`
  - app/toolchain packaging contract now matches the stronger scaffold shape:
    - Clang program outputs build under `build/toolchains/clang/`
    - `fisiCs` program outputs build under `build/toolchains/fisics/`
    - host test artifacts build under `build/host/`
    - desktop packaging rebuilds and copies an explicit
      `PACKAGE_TOOLCHAIN` source binary instead of whatever app binary most
      recently touched the shared `build/` tree
    - default desktop packaging still stays on the Clang lane
  - current sema customers:
    - `src/Tools/canonical_scene_export.c`
    - `src/Tools/canonical_scene_export_primitives.c`
    - `src/Tools/scene_import.c`
  - explicit scene authoring options now include:
    - `world_scale`
    - `unit_system`
    - `conversion_policy`
  - explicit primitive/export seam lengths now include:
    - primitive width / height / depth payloads
    - framing-bounds fallback half-extents
    - framing-bounds padding and bounds expansion
  - the import seam now validates:
    - supported `unit_system`
    - supported `conversion_policy`
    - numeric finite positive `world_scale`

## Recommended Agent Workflow
- For new room/object scene creation, treat `line_drawing` as the upstream
  authoring source and `ray_tracing` as the downstream inspection/render lane.
- The intended loop is:
  - author or revise a request JSON
  - optionally run `line_drawing/tools/agent_scene_refine.py` to normalize
    camera placement, camera aim, prism spacing, and light-path clearance
  - compile/export through `agent_scene_tool`
  - for mesh-object probes, include `mesh_asset_instance` entries with copied
    runtime mesh sidecars rather than hand-editing `scene_runtime.json`
  - inspect with RayTracing headless preview/material-preview lanes
  - feed approved material/light/camera changes back into the source request
- Default authoring guidance for current downstream compatibility:
  - normal scene objects should not be authored as emissive unless explicitly
    intended
  - transparent glass-like objects should use the transparent RayTracing preset
    (`material_id = 5`) in `extensions.ray_tracing.authoring.object_materials`
  - layered surface treatment should be expressed through
    `material_texture_stack` when object-level grime/oil/fog differentiation is
    needed instead of adding more one-off flat fields

## Structure
- Required lanes: `docs/`, `src/`, `include/`, `tests/`, `build/`
- Support lanes: `config/`, `data/`, `tmp/`, `external/`
- Active source subsystems:
  - `Core`, `Editor`, `Input`, `Layout`, `Math`, `Menu`,
    `ObjectAuthoring`, `Render`, `Tools`, `UI`, `app`

## Runtime Contract
- Default runtime ingress is the host menu, not the editor:
  - the host currently exposes five content sections:
    - `Quick Actions`
    - `Recents`
    - `Layouts`
    - `Scenes`
    - `Browse`
  - quick actions still expose resume, reopen-last-layout, reopen-last-scene, and quit
  - reopen-last-layout and reopen-last-scene now remember independent last-opened targets, so opening a scene does not replace the last JSON/layout reopen target and opening a JSON/layout does not erase the last scene reopen target
  - recents now mix recent layouts, scenes, input roots, and output roots into one activation lane
  - layouts/scenes now browse the current input root through an app-local catalog backend
  - layouts/scenes now support inline name/path filtering from the host surface itself
  - layout/scene rows now show lightweight metadata summaries plus compact projected wireframe thumbnails
  - the detail pane now renders a larger cached preview and preview-derived counts/extents for the selected layout or scene entry
  - the browse section now focuses on root context instead of general filesystem traversal:
    - explicit native picker buttons for input root and output root in the browse header
    - ranked nearby child/sibling/cousin directory suggestions that switch the input root toward likely scene-storage locations
    - nearby browse rows now resolve representative scene/layout preview targets so the list can show the same lightweight wireframe thumbnails used elsewhere in the host shell
    - activating a nearby browse suggestion now pivots directly into the corresponding `Scenes` or `Layouts` catalog for that root instead of leaving the user in a reordered browse suggestion list
  - mouse hover is now visual-only; committed section and row selection changes only on explicit click or keyboard movement
  - the list scrollbar now supports wheel scroll, track click, and thumb drag instead of relying on wheel-only movement
  - `Esc` now returns from the editor to the host menu when transient text-entry and authoring overlays are inactive
  - `Ctrl/Cmd + M` still returns from the editor to the host menu as an explicit chord
  - the in-editor JSON/scene picker remains an editor-local quick picker, not the top-level catalog surface
  - previews are app-local deterministic menu previews, not full editor-camera or downstream render thumbnails
- Runtime roots and persisted runtime-state lanes are explicit and normalized.
- Startup root hygiene/fallback behavior is active for input/output/layout roots.
- Legacy config fallback behavior remains for compatibility when runtime files are absent.
- Output-root export behavior is now user-visible and deterministic:
  - `Export Shape` still writes a single exported shape artifact
  - `Export Runtime` writes one receipt-bound package directory containing the
    package entrypoint, editable authoring, compiled runtime, dependencies, and receipt
  - export destinations come from the configured output root; active scene
    paths only influence the generated scene name
  - stored full-3D plane primitive metadata is normalized to the plane-locked
    scene contract during canonical export so older platform-plane layouts
    still compile

## Verification Contract
- Build/harness:
  - `make -C line_drawing clean && make -C line_drawing`
- Stable tests:
  - `make -C line_drawing test-stable`
  - includes `tests/test_scene_export.c` in the current worktree
  - includes `tests/test_layout_scene_export.c` option coverage for explicit
      authoring metadata
- Headless wording note:
  - `make -C line_drawing run-headless-smoke`
  - currently routes through `test-stable` rather than a separate runtime-only lane
- Build-only readiness:
  - `make -C line_drawing visual-harness`
- Source visual proof:
  - `make -C line_drawing visual-artifact`
  - renders one menu-first source-runtime frame through SDL/Vulkan and writes
    `line_drawing/visual_artifacts/line_drawing_first_frame.bmp`
  - expected success line:
    `visual-artifact: <absolute artifact path>`
  - `make -C line_drawing visual-artifact-editor`
  - enters the existing editor surface, renders one viewport/editor frame
    through SDL/Vulkan, and writes
    `line_drawing/visual_artifacts/line_drawing_editor_first_frame.bmp`
  - expected success line:
    `visual-artifact-editor: <absolute artifact path>`
  - requires local display-session access; if SDL reports no displays, rerun
    from a GUI-capable session rather than treating it as app logic failure
- Scene pipeline smoke:
  - `make -C line_drawing scene-pipeline-smoke`
- Packaging/release lanes:
  - `make -C line_drawing package-desktop*`
  - `make -C line_drawing release-contract`
  - `make -C line_drawing release-bundle-audit`
  - `make -C line_drawing release-sign ...`
  - `make -C line_drawing release-notarize ...`
  - `make -C line_drawing release-staple`
  - `make -C line_drawing release-verify-notarized ...`
  - `make -C line_drawing release-artifact-roundtrip-test`
  - the macOS bundle uses a native Mach-O launcher plus a sealed shell resource;
    final release ZIPs must pass post-extraction codesign, launcher self-test,
    detached-signature absence, Gatekeeper, stapler, and exact source-commit
    verification

## Current Boundary

- EVN2 normalizes viewport navigation behind an app-local typed contract:
  Alt/Option+LMB orbits without recentering, MMB pans the durable target in the
  camera screen basis, wheel/keyboard zoom preserve their anchor, and `F`
  frames selection or scene. Existing empty-space LMB pan plus handle, gizmo,
  topology, placement, modal, and splitter arbitration remain LineDrawing-owned.
- EVN3 adds a focused canonical parity group matching RayTracing's camera-basis
  pan and anchor-preservation invariants. CV3D3 now routes free-view pan,
  orbit, anchor zoom, frame, and resize transitions through a thin adapter over shared
  `core_viewport3d >= 0.1.0`. The adapter derives the effective 3D target from
  float-degree `FreeViewCamera`, Grid scale/offsets, and pane center while
  preserving LineDrawing's orientation-only orbit storage rule and effective
  world target when pane centers move during resize. Projection,
  selection/bounds resolution, input arbitration, authoring, picking, CPU
  raster/cache policy, overlays, and rendering remain app-local. The CPU mesh
  surface now supplies its existing depth plus object-owner buffer to optional
  shared `kit_viewport3d >= 0.1.0` for the same stable object-accent and
  silhouette/depth/owner outline roles used by RayTracing; projection,
  rasterization, quality/cache policy, SDL upload, and authoring remain local.
  Managed shared snapshot `a0714c0` now carries the matching core/kit
  contracts without a workspace-root fallback. The EVN2 contract remains a
  rollback oracle through CV3D4 hands-on acceptance.
- `line_drawing` is closed as upstream authoring/export source for current primitive scope.
- Current local drift now includes a durable app-host upgrade:
  - menu-first host split before the editor session
  - scene-directory export under output root
  - immediate runtime compile handoff
  - stable test coverage for scene export paths
- The next bounded local expansion is Phase 2 of the host lane:
  - deeper catalog ergonomics on top of the current browse/filter/preview shell
  - any remaining menu visual work should now be a smaller follow-up polish lane rather than another structural host-shell slice
- The structural editor shell lane is now complete:
  - preserve the left `Scene` + `File` context lanes and right-side state-card
    tabs as the default editor shell
  - future editor-facing work should enter as smaller usage-driven polish or
    capability slices on top of that shell
- Next major downstream boundary remains consumer-side integration (first in `physics_sim`, then broader trio consumers).

## History and Deep Lane References
- Full phase ledgers and archived slices are in:
  - `/Users/calebsv/Desktop/CodeWork/docs/private_program_docs/line_drawing/`
- This file is the compressed public current-state contract.
