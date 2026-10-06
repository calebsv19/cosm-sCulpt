# Furniture contacts: F2 connected runs

The F4 draft extends these same controls to five run links and nine units. See
[F3 / F4 editability](furniture_editability.md) for the current full-van handoff.

Implemented 2026-10-05 in persistent Main Edit. The native geometry transaction,
assembly frames, core units and math, existing history, theme, scroll/pane and
render invalidation are reused. Directed cabinet construction policy stays
app-local: core_scene/core_space describe broader scene transforms, not this
app's plywood recipes. No shared module, version or adoption minimum changes;
no idle solver or recurring preview rebuild is added.

## Full-van acceptance draft

`config/examples/van_construction_f2.layout.json` is a separate conversion of F1.
It retains every object, native geometry, relationship, route, saved view and
motion record. The converter adds recipes for the passenger pantry and upper run,
including its mounting rail, plus two explicit enabled links:

1. Kitchen rear end drives pantry front end: **Move unit**.
2. Pantry front end drives upper rear end: **Fit run**, retaining upper front end.

With kitchen Front end retained, increasing its length by 50 mm moves pantry
rearward 50 mm and extends the upper run and rail by 50 mm. Every affected
native panel keeps physical thickness and stable identity. The sink keeps its
own size and declared anchor. The shallow header's join remains fixed. Pantry
floor and ceiling heights remain unchanged; the driver-side units remain F0.

These are run-end plane rules, not a structural attachment solver. They constrain
one axial gap, without tangential alignment, face-overlap certification, wall
fitting, collision prevention, sink openings, wheel-well relief or fasteners.
The pantry's rear can move toward the bed/wheel wells. **Run the existing Checks
after changing layout**; the 1910 mm ceiling remains an unmeasured draft datum.
Routes are not invented or rerouted by these edits.

## Visible workflow

Select a managed panel, then Object -> Edit unit. **Connections +** expands a
compact form in the same pane. It shows only links touching the selected unit:

- Link cycles its saved connections; New link starts a separate draft.
- Driver and Follows cycle available managed units. Each has Rear/Front end.
- Move unit translates the follower assembly and all members rigidly.
- Fit run changes the follower length, retaining its opposite end. Managed
  panels and run rail regenerate; unrelated independent parts stay put.
- Gap accepts a unit suffix, from 0 to 1 m. Enabled/Disabled is explicit.
- Save link applies only the link draft and any required follower adjustment.
  Remove link removes the dependency, retaining current geometry.
- Apply unit applies dimensions and thicknesses, then all enabled followers.
  Cancel/back discards uncommitted fields and link settings.

Saved link operations and unit operations each use one Undo step. Undo/Redo
refresh displayed link settings as well as unit geometry. An unchanged Save or
Apply adds no history entry. Existing Save Layout behavior resets edit history;
Undo should be tested before saving. All fields use actual pane width; lists
and expansions share its existing scrolling. Links are collapsed by default.

A directly edited following end may remain constrained: retain that end, edit its
driver, or disable the link and Save link before changing it independently. A
standalone rigid follower move or incompatible rotation rejects instead of
silently snapping it back. A rigid move/rotation of the entire linked group is
supported. Attachment/containment relationships alone never make driving links.

## Data and validation

Schema 23 adds bounded `engineering.furnitureContacts` (maximum 32) and a
monotonic `nextFurnitureContactId`. Each contact has stable ID, driver/follower
assembly IDs, local run ends (-1 rear/+1 front), Move/Fit behavior, meter gap and
Enabled state. Schema 22 recipes still load without contacts; older readers
reject 23. Native recipe role 9 is a mounting rail with independent cross-section
and local offset whose length follows the unit. It is not inventory fiction or
an assembly scale change.

Enabled links require opposing parallel run planes, distinct managed units
outside one another's hierarchy, at most one incoming driver per unit, and an
acyclic graph. Incompatible existing geometric constraints, member locks, plane
or bounds locks, impossible interiors or failed history reservation reject the
whole candidate. No partial movement is published, even if a later follower fails.
Loading rejects a saved gap that disagrees with geometry beyond 2 micrometers;
it does not silently repair the authored document. Disabled links retain their
references and settings, but exert no movement and may be independently placed.

`Layout_EditFurnitureUnit` and `Layout_EditFurnitureContact` are the common C
command boundaries. General assembly/geometry transactions also maintain the
links or reject conflicts. Save, history and canonical authoring snapshots carry
recipes and links; renderer, sections and exports consume the same native parts.

## Validation and further work

`make test ARGS=Furniture` covers the real full-van 50 mm edit, both behavior
modes, thin-panel identity, mounting rail, physical scale conversion, a rotated
connected group, Undo/Redo, strict reopen/old-schema refusal, cycles, multiple
drivers, follower-edit rejection, early and late lock failures, gap edits,
disable/re-enable/remove, and actual native connection controls with history
refresh. `tests/test_van_contacts.py` checks conversion and baseline preservation.

F3 adds actual sink openings and compatible support geometry. F4 rolls out the
managed contract to driver furniture and desks. More general depth/top face
alignment, explicit datums and shared partitions remain separate work.
