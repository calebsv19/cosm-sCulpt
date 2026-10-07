# Furniture units: F0 through F4

Implemented 2026-10-05, extended through F4 on 2026-10-06 in the persistent Main Edit lane. See [F3 / F4 editability](furniture_editability.md) for the current full-van controls and opening contract. This is a bounded
construction editor, using the existing units, assemblies, geometry transaction,
history, scene export and solid/section renderer. No recurring rebuild is added.

## Full-van draft

`config/examples/van_construction_f1.layout.json` derives from the committed
130-object `van_connected_sections_s5.layout.json`. The accompanying review JSON
records its source hash, changed IDs, datums and explicit limitations. The source
working file is not overwritten. The Desktop draft uses the user's saved baseline
and retains its exact untouched values; the committed example uses its corresponding
repository baseline. All object IDs, 22 routes, motion records,
checks and saved views remain. Sink membership changes to the kitchen unit.

The kitchen case and worktop now meet the pantry's longitudinal boundary. The
bench meets the wardrobe. Both tall storage shells run from floor to the draft
finished ceiling at 1910 mm; upper runs end at their front boundary. The passenger
run steps shallower above the sliding-door reservation. The bed remains 1800 mm
transverse by 820 mm fore/aft, with its existing travel unchanged.

These are contact corrections between provisional rectangular furniture. Ceiling,
wall contours, ribs and installation allowance are unmeasured. Wheel-well cutouts,
ceiling scribing and door hardware remain unfinished. The historical F1/F2 sink
placeholder crosses uncut top panels; the F4 draft replaces that with real removed
material and a provisional hollow bowl. Contact is not a mounting or structural
certification. Existing checks remain available and still include hidden objects.

## Human controls

1. Open the full-van construction draft. View -> Visibility -> Furniture -> Only
   removes the other classified groups; Material or Solid reveals actual panel
   thickness and hollow spaces. Slice/Cutaway work on the same native panels.
2. Select a kitchen panel or the sink, then choose Scene -> Object -> **Edit unit**.
   This opens a compact form in the existing Object tab, with the old part controls
   hidden while editing. Input labels and bordered fields fit the actual pane width.
3. Enter Length, Depth and Height, with a unit suffix if desired (`900 mm`, `0.9 m`).
   **Length anchor** cycles Front end, Center and Rear end. The wall-side outer face
   and floor remain fixed. Untouched fields keep their exact stored numbers.
4. Open **Panels +** for Case, Back, Door and Worktop thickness. Only applicable
   roles appear: the kitchen's open-front shell has no Door field.
5. **Apply unit** updates all members in one validated Undo step, including enabled followers. Return also
   applies the form. **Cancel / back** discards the draft. Undo/Redo restore the
   whole recipe and all panel geometry. Invalid sizes or locks reject the edit
   before any partial changes are published.

The displayed Current inside L/D/H describes the applied unit, not the uncommitted
form. The fixed-size sink follows its declared wall/front/top anchor without being
scaled. A unit too small for the sink is rejected. The F1 draft has only the kitchen managed. In the F2 draft, passenger pantry and
upper run are also managed and follow their enabled links. Other panels retain
the existing numerical edit controls.

## Document and transaction contract

Schema 22 added `engineering.furnitureUnits`; schema 23 adds
`engineering.furnitureContacts` and `nextFurnitureContactId`. Schema 24 adds native openings and the cushion role; new saves use 24,
including empty arrays when unused. Earlier documents still load; readers older
than schema 24 reject new saves. Recipes are bounded to 16 units and 16 named parts per unit and
refer to stable assembly/entity IDs. Size order is local depth, run length, height;
all recipe dimensions are meters, relative to the assembly's rigid frame.

Role equations derive native unit-scale prisms: back, two ends, bottom, top,
optional door/shelf/worktop, fixed-size fixture and length-following mounting rail. Panel thickness is independent
of unit size. Back/door thickness determines clear depth; case thickness determines
clear run length and height. Worktop overhangs are separate recipe dimensions.
The recipe supports rigid assembly movement and rotation. Assembly IDs and member
IDs remain stable. Reparenting or individually changing/deleting a managed member
cannot silently break the record: conflicting edits reject with an Edit unit hint.

Loading validates recipe references and generated geometry rather than repairing
mismatches silently. Apply uses `Layout_RunGeometryEdit`, including existing checks,
constraint enforcement, history reservation and dirty notification. Failed edits
leave the live document unchanged. Export uses the same physical panel geometry;
the recipe is carried in the existing full authoring snapshot.

## Verification and next slices

`make test ARGS=Furniture` checks both wall orientations, physical scale conversion,
anchors, retained sink dimensions and IDs, no-op history, Undo/Redo, rigid rotation,
strict save/reopen, locked/invalid edits, independent-part drift rejection and
native input events for Apply, Cancel and actual pane splitter resizing.

`python3 tests/test_van_construction.py build/toolchains/clang/bin/physical_route_tool` checks the
committed full-van fit draft against its baseline, preserved protected geometry
and topology, native loading and create-only generator behavior.

F3 and F4 are implemented in the full-van draft. Next: the viewing/immersion pass,
then measured fitting, wheel-well cutouts, hardware and broader construction rules. See [Furniture contacts](furniture_contacts.md).
