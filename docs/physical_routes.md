# Physical routes

Implemented 2026-10-04. A Cable or Pipe is an app-owned engineering polyline with a
stable ID, name, extensible properties, two persistent entity endpoint references,
and ordered world-space points in meters. The current bounds are 32 routes per
scene and 64 points per route, including endpoints.

## Human workflow

Open the **Routes** tab. Tab labels wrap when the pane is too narrow to fit them;
they do not shrink to unreadable widths. The route selector opens the saved list.

1. **New route**, then select Cable or Pipe and enter a name.
2. Expand **Endpoints**. Choose Source and Destination from the object lists, or
   select an object in the scene and use **Use selected object**. This UI uses
   object centers; explicit local offsets are available in the native/CLI contract.
3. **Insert point** creates a midpoint. **Previous / Next** selects an ordered
   point, highlighted in the viewport. Enter its X/Y/Z using display units or an
   explicit suffix such as `25 mm`. Endpoint coordinates are read-only.
4. Optionally **Pick on construction plane** for an intermediate point. One click
   sets that point using the existing plane and grid snap. Escape or Cancel point
   picking stops this mode. It does not create a legacy line anchor.
5. **Delete point** removes an intermediate point. **Move earlier / later** changes
   its order. Endpoints stay first and last.
6. **Save route** accepts the whole draft as one Undo action. **Cancel edits**
   restores the saved route. **Save Layout** separately writes the document to disk.

The length readout is the sum of straight centerline segments in physical units.
The selected draft is bright teal; saved routes are dimmer. Amber means captured
endpoints differ from the currently resolved entity positions by more than 1 µm.
Refresh stages updated first/last points; Save route commits them. Interior points
stay in world meters. Object/assembly movement does not automatically reroute them.
Renaming or reparenting endpoint objects preserves references. Endpoint deletion is
refused until the route is removed or reassigned. Route deletion has a confirmation
control and participates in Undo.

## Native and agent contract

Native layout schema **20** introduced `engineering.routes` and `nextRouteId`. Older
schema 19 documents remain readable and gain an empty route list when saved.
Malformed route records reject the candidate without replacing the loaded scene.
Routes use the existing candidate validation and history boundary. Camera/light
animation paths retain their separate document representation.

Build `make physical_route_tool`. Inspect:

```sh
build/toolchains/clang/bin/physical_route_tool inspect input.layout.json
```

Edit using a structured JSON command and a **new** output path:

```sh
build/toolchains/clang/bin/physical_route_tool edit input.layout.json command.json NEW.layout.json
```

Command schema: `line_drawing_route_edit_v1`. Operations are `upsert`, `remove`, `refresh`, `default_views` and
`mark_corridor` (numeric `object_id`, design box only). For upsert, `route` has these fields:

```json
{
  "id": "",
  "name": "Water controller CAN cable",
  "type": "Cable",
  "parent": "",
  "reference": false,
  "volumeRole": 0,
  "volumeOwner": "",
  "properties": [{"key":"bus", "kind":0, "value":"can0"}],
  "source": {"entity_id":"can_hub", "local_offset_m":[0,0,0]},
  "destination": {"entity_id":"water_controller", "local_offset_m":[0,0,0]},
  "points_m": [[-0.44,1.25,0.39],[-0.865,1.25,1.88],[0.43,0.318833333,0.68]]
}
```

Empty ID creates a generated stable ID; existing ID updates that route. Remove and
refresh take `id`. Local offsets use the endpoint's primitive frame in physical
meters. Current endpoint support is plane/prism origins, not meshes or assemblies.
Inspection returns IDs, properties, captured points, resolved endpoints, stale
state and physical length. The CLI refuses existing output paths; failures return
structured error JSON on stderr. No MCP endpoint is added in this slice.

The authoring/runtime export retains this data under
`extensions.line_drawing.physical_routes`, explicitly identified as world polyline
centerline **metadata**. It does not produce tube geometry in rendering consumers.
Route properties can be inspected/edited through the CLI; the current UI exposes
name, Cable/Pipe kind, endpoints and points. Routes have their own list and are not
yet included in Parts' semantic query/filter list or assembly parenting.

## Example and acceptance

The [narrow van routing example](../config/examples/van_routing_s5a.layout.json)
contains one manually drawn CAN concept from the driver bench hub, up to the
upper driver halo, across the rear, along the passenger halo, and down to the
water-controller placeholder. Its approximately **10.370 m** centerline is a
layout exercise, not a verified CAN trunk/stub design or material cut length.
Corridor membership and containment are not enforced yet.

`make test` covers native history, atomic rejection, schema migration, endpoint
freshness, export metadata, unit conversion, visible controls and draft cancel.
`make route-smoke` covers the structured CLI, create-only output, exact point edits,
stale/refresh, deletion and reload. `make van-concept-smoke` checks the corrected
bed dimensions, section ordering, native min/max movement and saved checks.

## Next boundary

S5B adds editable named corridors, route membership and semantic filtering. S5C
adds segment-level clearance, reserved/motion/service volume and corridor checks,
with visible findings. Cable radius, bend radius, slack, electrical topology and
material calculations require additional explicit contracts. No automatic routing,
physics integration or manufacturing fit is claimed by this first polyline slice.

Shared reuse: existing `core_units`, primitive frames and scene compilation;
app-owned route policy and pane controls. No new shared module or shared version
change is introduced.

Schema 21 adds saved views and optional typed route design inputs. See
[routing views and electrical controls](routing_views_and_electrical.md) for the
current visible workflow, JSON/readback and check boundaries.
