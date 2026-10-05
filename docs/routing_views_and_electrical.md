# Routing views, corridors and tentative electrical layout

Implemented 2026-10-04 as the first S5B/S5C electrical foundation. This builds on
S5A manual polylines and preserves the existing scene, assemblies and motion.
The provisional van example is `config/examples/van_wiring_tentative_s5b.layout.json`.
It retains the narrow bed: 820 mm fore/aft width and 1800 mm transverse sleeping
length. Those are design assumptions, not verified vehicle or mattress dimensions.

## Visible controls

**Parts → Views** provides individual show/hide controls, **Only** to isolate one
query group, and **Show all** to clear transient filters. Definitions persist in
Save Layout; active hide/isolate state does not. Multiple enabled views combine
by union. Thus a cable in Wiring and 24 V remains visible while either matching
group is enabled. Use Only for a single voltage or subsystem. Unclassified entities
remain visible except during isolation. Authored object visibility is separate.

**Custom query / save view** exposes the existing type, designation, assembly and
property filters. Give the query a name, then Save query as view. Saving an existing
name replaces its definition; Remove and definition edits participate in Undo.
Add standard views is available for an empty list. Default Furniture matches design
primitives by type, not all descendants of an electrical or kitchen assembly.

**Routes → Corridors / limits** assigns named box regions and optional cable radius,
clearance and maximum centerline length. Create and size a design box through the
existing Create/Object controls, name it in Parts, then use **Assigned regions →
Mark selected box as corridor**. This explicitly changes its semantic type to
RoutingCorridor. Toggle region membership in the chooser and Save route. A route
can use up to eight regions. Moving or resizing a region changes subsequent checks;
region intent is not permission to pass through another object.

**Routes → Electrical** edits class (Unknown, DC power, Signal/CAN, AC power),
circuit, power domain, nominal volts, design amps, conductor area in mm² or AWG,
copper/unknown material, explicit return length, outgoing allowance and a tentative
fuse rating. Blank/zero numerical inputs are unspecified; they are not validated
zero-current designs. AWG is converted to conductor area when applied; entering an
area overrides an unsaved AWG entry. Save route details is one Undo action; Save
Layout writes the document. Route power-domain/circuit queries read these typed
fields, so changing electrical details also changes the matching views.

DC routes are amber (20 V and above) or blue (below 20 V); other routes are teal.
These colors indicate authored nominal voltage, not live measured voltage. Stale
captured endpoints use the existing amber warning. A hidden route remains available
in the selector and has an explicit Show route control.

**Routes → Checks → Check current draft** applies staged coordinates/details and
returns a read-only snapshot. Rerun after any route, geometry or motion edits. Click
an issue to select the target and highlight its segment in red; this also clears
transient view filters. Authored-hidden target geometry remains authored-hidden.
The pane shows up to 128 records and explicitly reports truncation.

## Check boundary

- Every segment must be covered by the union of its assigned oriented corridor
  boxes, shrunk by the declared radius plus clearance. Endpoint-only inclusion
  does not prove the interior is covered. Zero-length segments are handled.
- Maximum length compares total centerline length, not electrical return length.
- Centerline contact with a design primitive is an error. Radius/clearance padding
  expands each box axis conservatively; a padding-only contact is a review warning.
  Mesh bounds and current saved motion envelopes are conservative review warnings.
- Stale endpoint positions and stale motion envelopes are explicit warnings.
  Unsupported geometry is unresolved, not silently clear. Current motion checks
  use the saved envelope overview bounds, not an exact continuously swept mesh.
- Reserved/service volumes participate. Display filters and authored-hidden flags
  never exclude obstacles from checks. Reference/OEM geometry is not checked in
  this opening slice. Source/destination physical bodies are exempt, while their
  reserved volumes are not automatically exempt.
- Assigned corridors cannot be deleted or retyped while referenced. Endpoint
  deletion keeps the existing refusal. IDs survive renaming and reparenting.

A cable reaching a tap that is not its declared endpoint will currently be reported
as intersecting that connector. Cabinet/bench primitives are solid proxy boxes;
intersections with them identify review work, not an inferred wall penetration.
Explicit cable penetrations, declared intermediate electrical junctions and hollow
cabinet geometry remain follow-up work. No automatic pathfinder or bend-radius
solver is implemented.

## Electrical calculation boundary

For supported copper DC inputs:

`drop_V = design_A × 0.017241 × (centerline_m + outgoing_allowance_m + return_m) / area_mm2`

This assumes ideal equal-area copper outgoing/return conductors at 20 °C. The
coefficient is an explicit model assumption in Ω·mm²/m. Missing current, voltage,
area, copper material or positive return length yields unknown. AC and signal
routes do not use this DC calculation. Fuse rating is stored intent; ampacity,
protective-device coordination, interrupt rating, converter capacity, distributed
loads, connectors and temperature are not validated. A route drop is not a solved
network or a device-terminal voltage after all upstream feeders.

The research basis is [Victron Wiring Unlimited — DC wiring](https://www.victronenergy.com/media/pg/The_Wiring_Unlimited_book/en/dc-wiring.html):
current, both conductor lengths and cross-section are needed for sizing; 24 V with
DC/DC conversion for 12 V consumers is a documented architecture. Its AWG chart
provides comparison areas. Final cable/protection choices require equipment manuals
and the actual inventory and installation conditions.

For CAN, [TI's CAN physical-layer application report](https://www.ti.com/lit/an/slla270/slla270.pdf)
describes a linear 120 Ω twisted-pair bus with termination at its two ends, and a
0.3 m unterminated stub recommendation at 1 Mbps. That is a conditional planning
reference; the editor does not solve CAN timing or validate termination. A geometric
U-shaped route around the room is not an electrical ring. The example's rear stub
has an illustrative 0.3 m maximum, explicitly labeled with that rate assumption.

## Tentative van example

The fixture adds 13 saved views and 11 routes: the existing CAN main run, 24 V and
optional 12 V halo trunks, their feeders/converter feed, two local buck branches and
5 V outputs, and a rear CAN stub. Placeholder device sizes remain visibly tentative.
The 24→12 converter is in the driver electrical bench area; local 24→5 converters
sit near the water and rear ESP nodes. No hardware products or capacities are chosen.

| Comparison route | Explicit assumed load | Comparison conductor | Approximate route drop |
| --- | ---: | --- | ---: |
| 24 V U-shaped halo, about 9.09 m one-way | 10 A | AWG 10 / 5.26 mm² copper | 0.60 V / 2.50% |
| Optional 12 V halo, about 9.07 m one-way | 5 A | AWG 10 / 5.26 mm² copper | 0.30 V / 2.49% |
| Small 24 V local buck branches | 0.5 A each | AWG 18 comparison | Calculated from their own lengths |
| Short local 5 V outputs | 1 A each | AWG 20 comparison | Calculated from their own lengths |

These are example calculations, not recommended final gauges. The load values are
explicit assumptions; converter input current is left unknown where capacity is
unknown, and all fuse ratings remain unspecified. A later inventory will determine
trunk loading, branch protection and whether the optional 12 V halo is worthwhile.
The current report flags cabinet/bench proxy intersections and conservative rear
bed-envelope overlap. It is deliberately not a clean or construction-ready report.

Reproduce into a new directory:

```sh
make physical_route_tool
python3 tools/build_van_wiring.py --output /tmp/my-new-van-wiring
```

The script refuses an existing directory, preserves its input, validates through
the native CLI, and produces a layout, structured routing report and provenance.
`--input` can use an independently saved working layout with the same stable van IDs.

## Persistence, export and next work

Native schema 21 adds saved query definitions. Route `design` holds memberships,
limits and typed electrical inputs; schema 20 routes migrate with empty/unknown
inputs. Malformed definitions, invalid memberships and nonfinite or negative
numbers reject the candidate atomically. Export/CLI inspection include saved views,
route details, derived drops, explicit unknown state and bounded check reports.
Visibility controls never filter engineering export. Shared core_units, core_space,
core_scene/core_object and core_scene_compile retain their existing boundaries;
these document-specific engineering contracts remain app-owned and require no
shared ABI/version change.

Next: turn the illustrative junctions into explicit intermediate connectivity;
separate trunk sections and branch circuits for aggregate load/drop reasoning;
refine cabinet solids into walls and explicit penetrations; compare routes against
measured OEM boundaries; improve motion overlap precision with interval coverage;
and add a guarded inventory import/merge keyed by stable entity IDs. An inventory
should distinguish planned from selected parts and record quantity, input/output
voltage, continuous/peak load, measured/nominal dimensions, protection requirements,
location, circuit and source/manual references. Unknowns remain unknown.
