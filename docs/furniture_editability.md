# F3 / F4 furniture editability handoff

Implemented 2026-10-06 in persistent Main Edit. This closes the bounded furniture
resize lane before the next viewing pass. All dimensions remain provisional.

## Full-van acceptance example

`config/examples/van_construction_f4.layout.json` preserves 130 physical object IDs,
21 assemblies, 87 relationship records, 22 routes, 13 saved views and the existing
bed/door motion. Nine construction recipes cover the kitchen, passenger pantry,
passenger upper run, driver bench with cushion, floor-to-ceiling wardrobe,
driver upper run, shallow door-header upper, and both rear desks/support cabinets.
The five directed run links join kitchen/pantry, pantry/passenger upper,
bench/wardrobe, wardrobe/driver upper, and passenger upper/shallow header.
These links constrain run-end planes; they do not certify vehicle fit or strength.

Each rear desktop now belongs to its support-cabinet recipe, inside the existing
desk transform assembly. Neither board nor assembly identity changes. Length,
Depth and Height describe the support cabinet. Its desktop extends by four
independently editable overhangs, retaining its physical thickness. This is a
single-support desk construction, not an automatic leg/brace generator.

`tools/build_van_editable.py` creates a separate draft from a saved F2 file. It
validates the source and output natively, records the source SHA256 and changed
IDs, refuses existing output/review files and refuses unsupported moved/rotated
unmanaged assemblies. Existing managed kitchen placement and dimensions survive.
Do not rerun the older finite van generator over a manually edited working file.

## Visible editing controls

Select any managed panel, sink, desktop or bench cushion; Object -> **Edit unit**
opens the same compact form. Length/Depth/Height and the retained run-end anchor
remain the default controls. **Apply unit** is one transaction/Undo step including
all enabled followers. **Cancel / back** discards drafting. Failed edits publish
nothing. Individual dimension edits on managed members cannot silently break the
construction recipe.

**Panels +** shows applicable Case, Back, Door and Worktop thicknesses. Units with
a worktop also expose Wall, Aisle, Rear and Front overhangs. Units with a shelf
expose its height above the unit floor. Overhangs may be zero; shelf placement
must leave real clearance for its board. The bench cushion follows the bench
footprint and case top, retaining its thickness.

**Sink / opening +** appears only for the kitchen sink construction. Bowl width,
length, depth and center offsets from the cabinet wall/front are editable. Its rim
stays at the top of the worktop when cabinet height, worktop thickness or bowl depth
changes. The first rectangle uses a provisional 2 mm bowl wall/bottom and 2 mm
installation clearance on each side; it is not a manufacturer's cutout template.
The sink bottom must fit above the cabinet floor and the cutout must leave positive
material around both top panels. Oversized bowls/offsets reject atomically.

**Connections +** retains the existing driver/follower, end, gap and Move/Fit-run
controls. Apply does not automatically reroute wires or certify clearances. Run
the existing Checks after changing furniture near the bed, doors or service zones.
Save persists geometry and recipes; use Undo before Save, since the existing Save
workflow resets history rather than preserving Undo across sessions.

## Removed material and interchange

Schema 24 adds an optional typed `rectPrism.opening`: local U/V center, width,
height and retained floor in world units. Zero floor is a through-opening along
local N; positive floor makes a top-open pocket. The footprint must remain strictly
inside the prism. Old files without openings still load. Older readers reject
schema 24; downgrading the schema while retaining features also rejects.

The worktop and carcass top each remain ONE identifiable physical board with the
same rectangular through-opening. The sink is a hollow pocket with retained bottom.
No fictitious inventory boards replace these objects. Bounded evaluated cells are
internal query geometry only. Native surfaces, Material/Solid rendering, raster
ownership/picking, exact slices, cutaways, spatial distances and route obstacle
checks all consume removed material. Wire shows the opening boundary. Bounding
boxes and geometric face references intentionally remain outside datums.

Native board quantities include material volume with the opening removed.
Canonical scene export publishes one derived `mesh_asset_runtime_v1` dependency
per perforated object through the existing core_scene_compile dependency/bundle
pipeline. Translation stays on the object; vertices include the evaluated rigid
frame. The full native authoring snapshot retains editable recipes/features;
scene import restores that snapshot. The shared primitive schema currently has
no opening contract, so runtime consumers receive its existing solid-mesh carrier.
This reuses shared interchange without inventing an unrecognized shared primitive
or placing authoritative geometry only in the renderer.

## Validation and remaining boundaries

Host acceptance covers every unit's visible controls and resize/Undo, both inward
orientations and rotated recipes, sink edits, worktop/rim alignment, failed fit,
strict reopen/schema rejection, section area, empty raster ownership, distances,
route passage, signed exported material volume, real mesh loading and scene bundle
publication. Python migration tests check source preservation, unchanged unrelated
geometry/topology and create-only refusal. Desktop checks are recorded separately.

This lane does not add rounded/arbitrary cut profiles, wheel-well accommodations,
shared partitions, scribed ceilings, adjustable cushion thickness, manufacturing
joinery, fastener patterns, load calculations or full door/drawer mechanisms.
The next lane is viewing/navigation and immersion around this editable full scene;
measured construction and hardware detailing remain later work.
