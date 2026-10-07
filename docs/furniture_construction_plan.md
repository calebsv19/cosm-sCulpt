# Connected furniture and editable construction plan

Status: F0 fit corrections, F1 kitchen resize and F2 directed connected-run rules
implemented in Main Edit on 2026-10-05. See [Furniture units](furniture_units.md) for the actual controls,
persistence contract and limitations. F3 sink openings and F4 remaining unit editors are implemented on 2026-10-06; see [F3 / F4 editability](furniture_editability.md). The F2 draft
manages the kitchen, passenger pantry and passenger upper run, with explicit
kitchen-to-pantry movement and pantry-to-upper length fitting. The separately named F4 draft extends the managed controls to the driver furniture
and both rear desks/support cabinets.

## Intended outcome

The full van should show continuous, understandable furniture runs. Each cabinet
has a clean rectangular outside envelope and actual hollow construction inside it.
Editing the unit's length, depth or height updates its constituent panels together.
Panel thickness, countertop overhang, shelf height, sink placement and intentional
seams remain distinct physical dimensions. A coherent outside shape must not fill
the cabinet's usable interior with fictitious solid material.

Start with the existing passenger kitchen, adjacent pantry and overhead cabinets
in the full van. The isolated cabinet study remains useful for regression tests.
Once this example is usable, apply the same contracts to the driver bench,
wardrobe, desks and their cabinets. Preserve the narrow bed and entry reservations.

## Inspected baseline and concrete gaps

Main Edit source: `0c0f0e9016779da75108a423d873f271426d406e`, clean at inspection.
Scene: the user's saved `van_connected_sections_s5.layout.json` in the Main Edit
`projects/van_workflow` directory. Native schema 21; canonical meters, Z-up.
There are 130 objects, 21 assemblies, 87 relationships, 22 routes and 13 saved views.
These are provisional furniture and OEM proxies, not measured construction.

Existing capabilities to retain:

- Separate native solid panels, rigid nested assemblies and stable IDs.
- Numerical individual dimensions, panel thickness with a retained face, material
  appearance, measurement, saved travel/hinge and assembly movement.
- Solid/material depth visibility, exact native sections, cutaway, semantic views.
- Explicit relationship intent, obstacle/motion checks, route endpoints and Undo.
- Responsive panes and demand rendering; no recurring rebuild while idle.

The fixture's Python builder creates touching panels within individual shells,
but it is a finite generator, not an interactive cabinet resize operation.
Assembly movement is rigid; assemblies have no coordinated size edit. A panel
thickness edit does not resize adjacent panels. Relationships express intent;
they do not automatically enforce contact or resize connected furniture.

Read-only bounds observations from the current saved geometry:

| Observation | Current draft | Consequence |
| --- | --- | --- |
| Kitchen worktop to pantry along Y | Approximately 96.9 mm gap | Run is visually discontinuous; decide a joint/filler or extend the unit |
| Kitchen carcass to pantry | Approximately 116.9 mm gap | Worktop overhang and carcass span need separate definitions |
| Driver bench to wardrobe | Approximately 96.5 mm gap | Repeat the continuous-run treatment on the driver side |
| Passenger pantry and overhead | Shells share 150 mm vertically and overlapping X/Y ranges | Their occupied panels must be fitted without overlap |
| Sink membership | Sink is under `kitchen`, outside `kitchen_base_unit` | Moving only the cabinet does not move the sink |
| Sink bowl/top panels | Bowl placeholder crosses the uncut carcass top and worktop | A real opening and top construction must replace the intersection |

These observations are simple axis-aligned bounds calculations on this draft,
not a completed native construction validator or proof of vehicle fit.

The current upper carcass is 12 mm and lower/tall carcasses are 18 mm in the
fixture. Keep these as editable starting values, not selected plywood products.
The kitchen worktop's 40 mm proxy needs its own thickness specification.

## Proposed layout decisions

The accepted first-draft layout preferences are:

1. Continuous kitchen-to-pantry and bench-to-wardrobe runs, with intentional seams,
   rather than unexplained gaps.
2. Continuous upper storage toward the front, shallower above the passenger door,
   ending at the tall-storage section immediately before the bed.

Use those as draft defaults until refined by the user. Do not merge all
furniture into a single rigid block. Upper and lower units may share alignment
planes or mount to the same structure without moving together. The kitchen and
pantry remain independently identifiable modules joined at an explicit boundary.

User clarification (2026-10-05): the large opposite-side cabinet near the rear
axle, in the middle section immediately ahead of the bed, runs from the finished
floor to the finished ceiling. This explicitly applies to the driver-side tall
wardrobe. The upper cabinet runs extend to the tall cabinets and terminate there;
they do not continue above them. This gives the rear bed a tucked-away, enclosed
feel. The corresponding passenger pantry should meet the same longitudinal
transition; its final construction height can be refined with the paired layout.

This supersedes the earlier proposal to end tall storage at the underside of an
overhead cabinet. Resolve the current upper/tall overlap by shortening the upper
run to the tall unit's front boundary, preserving the tall cabinet's full height.
Treat that boundary as an explicit end-panel/joint, not overlapping cabinet shells.
Ceiling fit follows a declared finished-ceiling reference and installation
allowance; the maximum OEM box height is not a measured finished ceiling.

The first dimensional draft should share floor/base datums, front-facing planes,
wall-side setbacks and zone boundaries. Select actual values during the dimensional
review. Stepped depths above the sliding door need end/filler panels and a documented
transition; continuity does not require uniform depth everywhere.

Preserve the accepted rear bed width limit of 914.4 mm fore/aft, current 820 mm
width, transverse sleeping direction, paired desks, approximately 4 ft entry and
three approximately 3 ft zones. Fit decisions must account for the aisle, door,
wheel wells, service access and the full bed travel, including currently hidden
geometry. The maximum rectangular OEM boundary is not a verified wall contour.

## Construction contract

### Units, axes and identity

Give each managed unit a declared local frame with run length, inward depth and
vertical height. The UI uses those labels rather than exposing the legacy prism
U/V/N ordering. Driver and passenger units orient inward in opposite directions.
The contract must survive rigid movement and rotation of the unit.

Store a bounded typed construction recipe keyed by the existing assembly ID.
It contains outside dimensions, placement anchor, panel roles and IDs, thickness
assignments, shell arrangement, shelf parameters, explicit overhangs and fillers.
Do not pack the recipe into the eight available generic properties per entity.
Panel roles distinguish back, ends, bottom, top, shelf, face, door, worktop, filler
and mounting rail. Generated parts retain their existing stable IDs on resizing.

Keep one authored document: recipe parameters drive managed panel dimensions;
published native world geometry is the deterministic evaluated result consumed by
existing scene tools. Manual and managed modes are explicit. Loading validates
recipe references and evaluated geometry consistency; do not silently regenerate
or accept two disagreeing authorities. A schema change requires migration and
old-reader rejection rather than silently dropping construction data.

### Resizing and thickness

Support one unit first, then explicit neighboring-unit dependencies. Changing
outside dimensions recalculates panel spans and positions while preserving
physical thickness and the chosen anchor. Do not nonuniformly scale the assembly:
that would stretch plywood, hardware and sink dimensions.

A declared butt-joint arrangement determines which panels span the full length
and which fit between them. Separate case, back, door and worktop thicknesses;
show derived clear interior dimensions. Reject dimensions too small for the
construction, invalid shelves, inverted interior space and unsupported geometry.
Retain a selected wall face, end face or floor datum so resizing is predictable.

Managed parts cannot silently drift from their recipe through a low-level gizmo
or W/H/D edit. Offer the unit editor for dimensions and supported per-part
parameters (for example shelf height, thickness or appearance). An explicit
freeze-to-independent-parts operation can preserve current geometry and IDs for
manual customization; it must report affected fit rules before committing.

### Connections

Distinguish transform parenting, physical attachment intent and driving fit rules.
Start with simple directed rules: named unit face meets named neighboring face
with an explicit gap; align front planes; align worktop heights; fit tall-unit top
to the finished ceiling; terminate the upper run at the tall-unit front boundary. Users choose which unit drives and which follows. Ordinary
attachment links alone must not create these resize dependencies.

Separate cabinet end panels may meet at a seam. A shared partition is a different
construction option with one physical panel ID and ownership; it is later work.
Fillers and mounting offsets are real parts. Do not silently weld independent
panels or fuse cabinets. Detect unsupported cycles or conflicting locked faces.

### Kitchen and openings

Associate the sink with the cabinet's placement behavior so it follows a unit
move, while retaining its own dimensions and an explicit supported placement
anchor on the worktop. Resizing the cabinet must not stretch the appliance. Reject
or warn clearly when its placement, bowl, rim or service area no longer fits.
Keep the water tank, pump, devices and flexible routes separately explicit;
only declared dependencies follow furniture edits.

The sink opening is an identifiable feature of the worktop. Begin with a bounded
rectangular through-opening in one native panel. It drives actual visible surfaces,
section contours, picking, physical intersection queries and exported geometry.
Keep the physical board identity intact even if its derived mesh uses several
quads; four separate fictitious boards would corrupt construction inventories.
An opening annotation without removed material is insufficient acceptance.

The current solid top underneath the worktop must either receive a compatible
opening or become an explicitly modeled support-rail arrangement. Confirm the
construction instead of leaving hidden solid material through the sink. Actual
sink flange/cutout dimensions are unknown; label the first rectangle provisional.
Rounded openings, arbitrary profiles, doors/drawers, joinery and fastener details
can follow after this first panel feature works throughout the consumers.

## Human editing workflow

Use the existing Object tab; add no top-level tab or keyboard-only workflow.
Selecting a managed panel exposes a clear **Edit unit: Kitchen** action. A compact
unit editor shows Length, Depth, Height, Keep face and interior dimensions, with
**Apply / Cancel**. A collapsed **Panels** group exposes thickness/part settings;
a collapsed **Connections** group shows adjoining units, faces and seams.

Give the unit a distinct outside preview outline. Show changed parts and the
retained face while drafting; provide concise conflict messages naming the part
or rule. Selection must clearly distinguish the whole unit from an individual
panel. Eventually add unit-boundary drag handles on the same command boundary;
start with the numerical form so precise dimensions can be validated first.

All rows use the current pane width, measured text and existing scroll/layout
invalidation. Generate the preview on input changes and checks on bounded edits,
not on every idle frame. Typing/drafting is cancelable. One successful unit edit,
including declared followers, is one Undo action. Save/reopen restores the same
recipe, panels and connections. Both human and agent edits use the same operation.

## Implementation sequence and acceptance

| Slice | Deliverable | Acceptance |
| --- | --- | --- |
| F0 - fit map | Full-van dimensional/contact audit; chosen run boundaries and intentional gaps | Every unexplained gap/overlap in the example has a proposed remedy; protected entry/bed/aisle boundaries remain explicit |
| F1 - unit resize | Typed construction record and compact UI for the existing kitchen shell | Change length/depth/height and thickness; all case panels fit, thickness is retained, stable IDs and undo/reopen work |
| F2 - connected run | Kitchen/pantry and corresponding uppers aligned by explicit directed face rules; counter and sink placement follow declared anchors | Units meet without accidental overlap; full-height wardrobe fits the ceiling and uppers end at tall storage; a conflicting edit rejects atomically |
| F3 - usable sink | Actual worktop opening and compatible support construction | Solid and slice show empty cutout/bowl space; picking, checks and export agree with the removed material |
| F4 - whole-van rollout | Driver bench/wardrobe and rear desk units use the proven contract; upper continuity/fillers refined | Full van reads as coherent furniture; protected motion/service space remains checked; the same editor works for each unit |

F1 is a complete UI-and-data slice, not a backend-only milestone. F2 may add the
sink placement anchor before F3 adds removed material; show any remaining bowl/top
intersection explicitly and do not call that intermediate kitchen finished.
F0 and F1 are the recommended first implementation batch. Keep the existing user
scene untouched while preparing a separately named construction draft; review
ID-based changes before promoting it. Do not rerun the old generator over a user
working file or overwrite its manual edits.

Regression and desktop acceptance should include unit resizing in both inward
orientations and a rotated fixture, altered thickness without opened seams,
locked/conflicting parts, whole-batch failure, dependency cycles, recipe migration,
manual-part protection, ID preservation, undo/redo and save/reopen. For the full
van, inspect Material, Wire and orthographic sections, narrow/wide panes, visible
unit selection and changed geometry, motion envelopes and route findings. Moving
device mounts invalidates relevant route checks; never silently invent new cable
paths. Measure idle CPU again after construction previews are added.

## Architecture and scope

Reuse adopted core_units/core_space/core_object, the existing core_scene and
core_scene_compile interchange, native geometry/validation/history, and kit pane,
font, theme and rendering adapters. Shared math and unit meaning remain shared.
The initial bounded cabinet policy, construction recipes and editor UI are
app-owned (reuse-deferred for a new generic CAD module). Generic panel feature
semantics should be reviewed for core_scene reuse/extension during F3; do not hide
unsupported authoritative openings in a renderer-only mesh patch.

No new scheduler, worker pool, database, generic constraint solver or rendering
backend is required for the first bounded batch. Runtime/time infrastructure and
shared data/storage APIs remain existing adopted paths; recipe persistence uses
the scene's document extension boundary. No shared version/API/adoption changes
are made by this planning document.

Source starting points: [assembly contract](semantic_assemblies.md),
[panel thickness and sections](solid_sections.md),
[fixture construction](../tools/build_van_sections.py),
[unit/identity types](../src/Layout/layout.h),
[assembly mutation boundary](../src/Layout/layout_engineering.h),
[panel surfaces/thickness](../src/Layout/scene/layout_section.c),
[Object inspector](../src/UI/panel/ui_panel_object_inspector.c),
[Parts forms](../src/UI/panel/ui_panel_parts.c),
[responsive panes](responsive_panes.md), and
[accepted narrow layout](van_layout_narrow_v2.md).

Storage preflight: current bounds are 32 assemblies, 128 relationships and eight
properties per entity. The fixture already uses 21 assemblies and 87 relationships.
Reserve capacity before multi-unit edits or revise bounded storage deliberately
with compatibility tests; reject overflow atomically. Contact tolerances must be
explicit and tested at meter-scale coordinates; current float geometry is not a
manufacturing tolerance guarantee.
