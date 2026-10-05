# Connected route sections and panel furniture

Updated: 2026-10-05. This is the next bounded S5 slice, built on schema 21,
stable entity IDs, existing assemblies/Links, physical routes and saved views.

## Viewport

Ordinary Scene plane/prism outlines use a single logical screen stroke. Decorative
face diagonals and unselected plane corner dots are removed. Selection/hover and
resize handles retain stronger feedback. This changes display, not dimensional
geometry, hit tolerance or exported meshes. Mesh/Object topology modes retain their
existing authored edge behavior.

## Connections and section editing

Routes has a collapsed **Connections / sections** group. It names both endpoints
and lists other sections sharing each endpoint. Click a section to inspect it.
Crossing polylines do not establish connectivity. Passive junctions are explicit
Connector or PowerBus entities; converters/controllers are devices, whose electrical
input/output port behavior is not inferred from spatial contact.

To split, select a saved route, use Previous/Next to choose an interior point, then
open Connections and choose **New junction at this point**. It creates a 16 mm
concept connector and two routes in one Undo. **Split at selected junction** uses
an existing Connector/PowerBus, whose origin must match within 0.001 mm. Save draft
changes first; a split never silently saves/throws away an unfinished form edit.
An existing endpoint cannot be used as a split point. Stale endpoints and exhausted
route/metadata capacity reject atomically.

The original ID stays on the source section; the destination section gets a new
stable ID. `section_of` preserves the original family through repeated splits.
Captured points remain unchanged. Explicit return length is divided in proportion
to section centerline lengths, outgoing allowance stays on the source section, and
the prior whole-route maximum length is cleared on both sections. Review limits
and loads after splitting. With equal unchanged current/area, section drops sum to
the prior ideal drop; this is not a solved distribution network. Unknown inputs
remain unknown. Junction deletion is refused while referenced.

Inspection/export includes a `connections` incidence array with entity ID, name,
section IDs and endpoint roles, degree, passive-junction status, domain conflicts,
port-offset differences and stale-section state. Checks report `junction_domain`
for incompatible authored circuits, power domains, voltages or signal classes at
passive junctions, and `junction_ports` when shared IDs resolve to different local
ports. Unknown values do not establish compatibility. Geometry/checks ignore
visibility filters. Device conversion, loads, termination and internal connections
remain explicit future contracts.

Native create-only command:

```json
{
  "schema": "line_drawing_route_edit_v1",
  "operation": "split",
  "id": "route_2",
  "point_index": 1,
  "junction_id": ""
}
```

`point_index` is zero-based. Empty junction ID creates; a supplied ID reuses.

## Subsystem inventory

`physical_route_tool inspect` includes `inventory` with schema
`line_drawing_inventory_v1`: stable entity ID, object/assembly/route kind, name,
type, assembly, subsystem, part number, status, observed prism dimensions or route
length, and explicit voltage/current assumptions. Unspecified numeric values are
JSON null, not zero-load claims. Object dimensions are local prism extents times
scale in meters; they are not measured bounding-shell claims.

A guarded `inventory` edit patches only existing IDs. Editable keys are name,
subsystem, part_number, status, power_domain, nominal_volts and design_amps. Omitted
keys stay unchanged; blank text removes that property, and numeric null removes an
assumption. Values must be finite and positive when supplied. Duplicate/missing IDs,
read-only/unknown columns and metadata capacity overflow reject the whole candidate.
Imports change no geometry, parenting, route class, protection or conductor area.
Names/properties use the existing Parts controls, and route electrical assumptions
use Routes. Every native batch is one history transaction; CLI outputs remain
create-only. This is an inventory data foundation, not an Excel importer or native
spreadsheet editor. Exported observations are not themselves patch commands.

```json
{
  "schema": "line_drawing_route_edit_v1",
  "operation": "inventory",
  "items": [
    {"entity_id": "water_controller", "subsystem": "water", "part_number": "TBD"},
    {"entity_id": "route_7", "design_amps": 0.5}
  ]
}
```

No current is automatically propagated through upstream sections. No selected
parts, rated converters, wire ampacity or protection coordination are asserted.

## Revised van example

[Connected van](../config/examples/van_connected_sections_s5.layout.json) has
130 objects, 21 assemblies, 87 attachment/support/containment links, 22 routes and
13 saved views. [Inventory snapshot](../config/examples/van_subsystem_inventory_s5.json)
retains its stable IDs for later parts-list mapping.

The battery bench, kitchen, upper cabinets, tall storage and drawer pedestals are
hollow panel assemblies. The old proxy ID stays on a backing panel; the corresponding
`*_unit` assembly now represents the whole cabinet. Shelf, door, bottom, top and end
panels are separately editable. Panels contact one another within 1 micrometer in
the fixture's axis-aligned contact audit. This proves geometric continuity, not
fastener details or load capacity. Bench/kitchen fronts remain open for access;
tall/upper cabinet doors are thin fronts whose hinges remain to be designed.

Drawer tops meet the desk undersides. Desk legs reach the floor and join their tops;
new transverse bed rails join the existing side rails and support the deck. Wall
mounting spacers join upper cabinets to the nominal side-wall boundary. Parts
Assemblies moves each cabinet/desk as a unit; Links exposes support/attachment
intent. Unrelated furniture groups do not automatically fuse or follow each other.
Space between zones and door/aisle reservations is retained.

The bed stays 1.80 m transverse × 0.82 m fore/aft. Its native conservative envelope
is regenerated with the added moving frame members (six members, sixteen samples).
Fixed guides remain outside the moving assembly. OEM extents and furniture sizes
are provisional; no measurement accuracy or structural attachment suitability is
asserted by these links.

24 V is split into five halo sections at rear turns, the rear tap and water tap.
Optional 12 V has three sections. CAN has six sections with its own rear/water
junctions; it no longer shares the 24 V rear connector. The three-way water/rear
power taps and rear CAN stub are now incident to their actual trunk sections.
Signal routing is offset from power geometry for readability, without asserting
installation separation compliance. Existing feeders and local buck outputs remain.

Generate a new copy without overwriting input or output:

```sh
make physical_route_tool
python3 tools/build_van_sections.py --output /tmp/new-connected-van
```

Outputs: layout, routing report, inventory snapshot and provenance with prior
furniture envelopes/source digest/contact audit. Optional `--input` accepts the
previous S5B fixture or an independently saved working copy with the same IDs.
The fixture is finite, not a general cabinet generator/repair operation.

Native `refresh_motion` commands take constraint `id` and integer `samples`;
the generator uses the existing envelope mutation boundary after adding bed rails.

## Remaining physical design

Route checks still report actual panel/device intersections, missing protection,
unknown converter loading and conservative bed-envelope overlaps. Hollow furniture
removes proxy-filled interiors, but does not invent cable pass-through holes.
Named penetrations, actual mounting hardware/steel, cabinet doors/service access,
measured OEM geometry and refined motion intervals are the next physical boundary.

Electrical network load/drop accounting needs explicit source ports, conversion
behavior and branch load provenance. A workbook/CSV adapter should map into this
stable-ID patch contract and preview differences before accepting edits; it should
not replace the scene model or overwrite dimensions from unverified render labels.

Shared reuse: core_units/core_space/core_object/core_scene/core_scene_compile and
existing kit drawing/UI remain adopted. Section, inventory and fixture policies
are app-owned; no shared ABI or VERSION changes were needed.
