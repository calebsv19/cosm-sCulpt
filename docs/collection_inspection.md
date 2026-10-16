# Collection dimensional inspection (Main Edit development)

In **Visibility → Scene**, select an area's label, then **Inspect area**. Measure
opens a collection inspection pane. **Area** switches the scope; **Only + frame
area** isolates and frames its complete envelope while retaining independent layer
exclusions. Camera orbit and viewport selection remain available.

Inspection bounds include authored-visible physical members, including future
parts assigned through collection or assembly membership. Temporary visibility,
Only and covering filters do not shrink these dimensions. Authored-hidden obsolete
parts, deleted objects and nonphysical guides are excluded. Unsupported physical
geometry makes the area result incomplete instead of displaying partial dimensions.

## Frames and dimensions

The initial frame is World X/Y/Z. **Use viewport part frame** chooses the selected
primitive or STL instance's origin and rigid axes. **Use Parts selection frame**
accepts the selected native object or assembly. **Use world frame** resets it.
Changing this setting is a document edit with Undo and requires File Save.
Missing, ambiguous, degenerate or nonorthogonal frames are reported; no silent
fallback to World occurs. Frame configuration does not change motion parenting.

U/V/N ranges and extents are measured in the chosen frame. World AABB spans remain
World X/Y/Z. The part selector shows its local dimensions, world AABB and origin
position in the collection frame. Primitive dimensions use authored U/V/N sizes;
STL local sizes use stored mesh bounds and instance scale. Rotated/scaled STL
boxes are conservative envelopes, not fitted surfaces or manufacturer measurements.
All internal lengths are meters; Units changes display only.

## Saved measurements

Choose **Area extent U/V/N**, **Point distance A to B**, or **Signed projection U/V/N**.
For datum measurements, select a part and use **Use part as datum A/B**. Features
are Origin, Envelope min corner, Envelope center and Envelope max corner. These
stable datums follow the object's authored local axes and current geometry; they
are not preview triangles, named holes or arbitrary mesh-surface picks.

Enter a name, then **Save measurement**. New/Update/Delete are single Undo edits.
Open **Saved measurements in this area** to select an existing record. Saved
records pin collection, entity and frame IDs plus feature and operation; values
are recomputed for the current pose. Changing the area's frame later does not
retarget an existing measurement's captured frame. Deleted/missing/out-of-scope
references remain inert and display an error. Extents update with current physical
membership; record evaluation never drives or moves geometry.

The viewport draws envelope-reference endpoints and a current-pose readout. These
annotations are X-ray aids, not minimum surface clearances. Use **Return to
reference / motion tools** for the existing constraints and mechanism controls.

## Save/reload and compatibility

Inspection, framing, display units and reading measurements do not create Undo or
change geometry. Saving a frame or measurement does. Save As a new document when
adding these records to an older master. Documents with inspection frames or saved
measurements use layout schema 29; documents without them retain schema 28.
Older app builds cannot read schema 29. The bounded store holds 128 measurements.

The supplied driver-side pilot retains the original 890 objects and motion, adds
four saved measurements and an explicit frame, and is a separate document. Current
area bounds are approximately 602 × 3020 × 1910 mm; those are the modeled envelope,
not a cabinet cut list. The construction-reference exporter now creates model-derived sheets; tolerances, hardware fit and fabrication qualification remain physical review work.

## Typed controls

`layout_edit_tool inspect INPUT.layout.json COLLECTION_ID` prints current frame,
physical-part inventory, local dimensions and saved measurement results in meters.
No source document is written. `line_drawing_layout_edit_v1` supports
`inspection_frame` (`collection_id`, `frame_entity`), `save_measurement` (`id`,
`name`, `collection_id`, optional `frame_entity`, integer `kind` 0–6, and `a`/`b`
objects with `entity` and `feature` 0–3), and `remove_measurement` (`id`). Extent
records omit A/B; omitted frame_entity explicitly means World. Native and typed
controls share the same evaluator and metadata transaction.


## Construction references (Pass 3)

**Export construction reference sheets** creates a fresh `construction_reference_NNNN`
directory inside the File output root. **Open last exported sheets** opens the
printable HTML packet in the default browser. Print at A3 landscape or save a PDF;
use written dimensions, never measure a fitted diagram. Export does not change
geometry, saved measurements, visibility, dirty state or Undo. Repeated exports
create separate directories and never overwrite earlier sheets.

The packet includes U/V, U/N and V/N orthographic placements, assembly V/N details,
a saved-measurement schedule, and a numbered inventory with native local U/V/N
sizes, frame minimum/maximum coordinates, assembly identity and authored properties.
Assembly details contain directly parented members; whole-area projections include
all physical descendants. Guides and authored-hidden obsolete members are excluded.
Temporary visibility exclusions do not shrink the specification. Unsupported area
geometry or unresolved area frame refuses export; broken saved measurements appear
as errors with no numeric result. Rendered material is not a stock specification.

`specification.json` retains canonical meters and transformed native corners.
`source_snapshot.layout.json` records the exact current model, including unsaved
edits and motion pose. Asset references may remain external: retain the original
project assets. `COMPLETE.txt` is written last; incomplete invocation output is
removed after failure. Export is a model reference, not an approved fabrication
cut list, stock purchase list, surface clearance report or mounting design. Source
statuses such as `concept_not_measured` and `plywood_tentative` are carried through.

Typed create-only export:

```
layout_edit_tool reference INPUT.layout.json COLLECTION_ID NEW_DIRECTORY
python3 tools/construction_reference/render_pdf.py NEW_DIRECTORY NEW_PACKET.pdf
```

The optional PDF renderer requires ReportLab, reads the same completed specification,
adds the snapshot SHA-256, and produces A3 landscape sheets with three projections
per assembly detail. Both routes refuse existing outputs. The native HTML output
works without Python. No layout schema change is required by export.


### Driver cabinet fit revision v2

The separately saved driver fit revision removes the articulation expansion's
duplicate full back and stacked shelf, retains bed-recess panels and drawer depths,
fits upper wardrobe shelves to the backing, trims added overhead internals to the
carcass front, and adds explicit runner ledgers plus 3 mm stay/4 mm hinge mounting
spacers. Mounting dimensions are provisional; fasteners, bores, hardware selection
and loads remain unqualified. The native managed run rail accepts explicit positive
run spans, with zero retaining legacy full-run behavior. This candidate requires
the current Main Edit rail-aware build; older builds may refuse its managed geometry.

The finalized v2 adds a stepped 18 mm lower-drawer back and a three-section return
notched around the wheel-bridge timber, trims adjacent shelf edges for butt joins,
and preserves the bed's 0.70 m lift path. Its six saved measurements include three
area extents and explicit lower-rear (3 mm), side (10 mm) and upper-rear (32 mm)
clearances. The old generic 18 mm datum referenced the retired duplicate back and
is removed only from v2; the original inspection document remains unchanged.
