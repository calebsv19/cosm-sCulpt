# Native saved-layout edits and colored device groups

Build `make BUILD_DIR=build/<fresh-task> layout_edit_tool`. Invoke:

```
layout_edit_tool edit INPUT.layout.json COMMAND.json NEW.layout.json
```

Native documents support up to **128 assemblies** in this Main Edit build.
The same bound governs load, edit, save, hierarchy validation and viewport use.
A 129th group is refused atomically. Earlier 32-assembly builds cannot open scenes
that exceed their limit; use the matching qualified tool and review app together.

The command schema is `line_drawing_layout_edit_v1`, with an `operations` array
(up to 256). The tool loads through the native parser, applies the native geometry,
engineering and furniture APIs, saves and reloads in memory, then creates the
output exclusively. An invalid operation aborts the batch without publishing a
candidate; an existing output is preserved. Input mesh files and input layouts
are never modified. This is an offline authoring route; interactive edits retain
Sculpt's ordinary history/assembly controls.

Operations:

- `object`: `object_id` is the persistent engineering ID. Optional
  `dimensions_m: [width,height,depth]`, `position_m: [x,y,z]`, `visible: bool`,
  and `finish` modify a physical object. Position follows the native setter's
  center convention for meshes. Locked objects and constrained edits are refused.
- `replace_mesh_asset`: `object_id` and an absolute `runtime_mesh` path.
  Replaces only a mesh instance's validated SI asset binding through the native
  geometry transaction. Persistent/numeric IDs, pose/scale, parent, flags,
  material, measurement references and identity allocation are preserved.
  Non-mesh, locked, ambiguous/missing identities and invalid assets are refused;
  failed batches publish no output. Keep the old asset and use a new authored
  mesh for a size change instead of stretching the imported object.
- `resize_furniture`: `assembly_id`, `size_m: [depth,run,height]`.
  Regenerates managed panels and attachments while preserving part IDs, the
  outside wall face and floor. Optional `retain_run_end` is `-1` (rear), `0`
  (center, default), or `1` (front); it must be an integer. The native transaction
  shifts local `centerMeters[1]` by `-retain_run_end * (newRun-oldRun)/2`, regenerates
  the parts and solves existing furniture contacts together. No separate
  `move_assembly` is needed for that retained-end resize. Contacts remain active;
  incompatible driving constraints still reject the entire edit. Use this for managed
  bench/cabinet parts rather than editing individual panels.
- `add_prism`: `name`, `dimensions_m`, `position_m`, optional `finish`.
  Creates a full 3D XY prism. The result reports its generated persistent ID.
- `add_mesh_group`: `name`, `position_m`, `rotation_deg: [x,y,z]`, and `parts`.
  Each part needs `name`, an absolute `runtime_mesh` path, and `finish`.
  Runtime vertices must be in metres and share the device source origin.
  Scale is exact (`1 / document world scale`), never fit-to-scene. Rotation uses
  world X, then Y, then Z. Native assemblies retain all parts as one rigid device;
  offset part bounds do not change the common source origin. Result rows report
  the generated assembly ID and member IDs. Repeated names are display labels,
  not persistent identities. Existing placeholder IDs can be hidden and retained;
  the new group has a separate native assembly ID.
- `move_assembly`: `assembly_id`, `translation_m`, `rotation_deg`.
  Uses the same atomic rigid move as the editor; rotation is about assembly origin.
- `add_inspection_camera`: `name`, `eye_m`, `target_m`, `fov_deg` (10–150).
  Adds a native camera and one-second stationary review path with pose keys.
  Result reports `camera_id` and `path_id`. Existing cameras/paths are retained.
  The optional `LINE_DRAWING_CAMERA_EXPORT_CLEAN=1` omits outline overlays.
  The existing `camera_path_tool` and explicit native camera export route in
  [camera_view.md](camera_view.md) produce portable bundles and viewport captures.

A `finish` is `{ "rgb": [0.015,0.39,0.64], "roughness": 0.38,
"reflectivity": 0.2 }`. All channels are finite 0–1 values. RGB is quantized to
8-bit channels. Explicit solid finishes are independent of engineering properties
and physical materials, persist as optional `surfaceFinish` engineering metadata,
and are used in Material viewport mode and canonical/native renderer exports.
Each custom object gets its own export material ID. Selecting a finish preset
clears the custom override. Reflectivity is the renderer's control, not a PBR
metallic value; callers must choose a conversion explicitly.

STL does not carry these colors. Prepare material-separated runtime meshes using
the shared import/compile route, or use the unit-aware STL import documented in
[stl_device_import.md](stl_device_import.md). This tool does not add STEP or GLB
import or infer colors from STL facets. Old app builds do not know custom finishes;
use the qualified review build to edit or re-save documents containing them.

Verification: `tests/test_layout_edit_tool.py EDIT_TOOL VAN_CONCEPT_TOOL` covers
native grouping/movement of off-center meshes, color persistence/export, camera
addition, input preservation, failed batches and overwrite refusal. Engineering
unit tests cover custom finish history and validation, and nested rigid moves
with offset source bounds.

## Typed native hinge and motion operations

Hardware revisions can use `detach_telescoping_stay` with `rule_id` to retire
an existing illustrative follower while retaining the door hinge and its scope.
`set_hinge_offsets` takes `fixed_offset_m` and `moving_offset_m` together, so
both pivot references can move without shifting the closed leaf. It refuses
hinges with an attached brace or stay; detach the obsolete mechanism first.
`set_motion_scope` takes an existing `motion_assembly` and routes through the
ordinary atomic constraint validation. The scope must contain the moving datum
and exclude the fixed datum. None of these commands establishes hardware force,
friction, positive closure or load capacity.

These operations are implemented in `src/Tools/layout_edit_motion.c` and use the
existing native solver. They share the create-only batch/publish behavior above.
No new app runtime or saved motion format is introduced.

- `create_hinge`: required unique nonempty `rule_id`, `fixed` and `moving`
  references, exactly one `axis` or `plane`, `min_deg`, `max_deg`, `position_deg`.
  Optional `motion_assembly` is an existing assembly ID or empty for B only.
  Existing IDs are refused; this operation cannot replace another rule.
  The native initializer captures home/frame from current geometry, then the
  solver applies the requested initial Position. The result returns `rule_id`,
  captured `home_deg`, and `position_deg`.
- `create_travel`: the same `rule_id`, `fixed`, `moving`, `axis` or `plane`,
  and optional `motion_assembly` fields, with `min_m`, `max_m`, `position_m`.
  The native initializer captures the current attachment frame and rest offset.
- `set_travel_position`: `rule_id` and `position_deg` for a Hinge, or
  `position_m` for linear Travel. The wrong-unit field or both fields are refused.
  Saved limits and driving rules remain active. Result returns the rule/position.
- `generate_motion_envelope`: `rule_id`, integer `samples` 2–129 (33 recommended).
  Uses isolated native sampling and conservative hinge padding. Existing envelope
  identity is retained on regeneration. Result returns `rule_id`, envelope
  persistent `object_id`, and `current`. Pose changes inside the range do not
  stale the envelope; changing its driving references/range/scope/shape does.

Each reference is `{ "entity_id": "...", "feature": "axis_v",
"offset_m": [0,0,0] }`. `offset_m` is optional (zero), measured in the reference
object's local U/V/N basis, and rotates with the object. Features are `axis_u`,
`axis_v`, `axis_n`, or prism faces `face_pos_u`, `face_neg_u`, `face_pos_v`,
`face_neg_v`, `face_pos_n`, `face_neg_n`. A feature contributes both a direction
and its native reference point; a face starts at that face center. Origin-only
references cannot define the hinge orientation and are not accepted.

`axis` is a finite nonzero world vector, normalized by the parser. `plane` is
`xy`, `yz`, or `zx`, giving positive normals +Z, +X or +Y. Use an explicit axis
for the opposite sign. Native directions must be coplanar; limits are in
(-180,180] and must contain the captured home and requested Position. Creation
joins the two offset pivots; align them deliberately before authoring.

A moving scope supports at most 64 design planes, prisms or imported mesh
instances, contains B and excludes fixed A. A and B still use native primitive
attachment frames; imported peers retain their source origin and rigid offset.
Reference/reserved members and other geometric rules on scoped members are refused.
Moving an enclosing assembly transforms a wholly enclosed motion rule with its
attachment frames; cross-boundary constraints keep their world-space contract.
The Measure tab retains the active free camera while operating saved motion.
Native saved-envelope generation currently refuses imported mesh members atomically;
use conservative transformed asset bounds for their clearance review. Managed furniture recipes remain authoritative; this route
does not detach or disable their contacts. A standalone leaf is the smallest
supported worktop. Envelopes are conservative coverage, not hardware strength or
exact swept-solid acceptance.

For the sink precursor's horizontal leaf `obj3d_239` and strip `obj3d_240`:

```json
{
  "schema": "line_drawing_layout_edit_v1",
  "operations": [
    {
      "operation": "create_hinge",
      "rule_id": "worktop_hinge",
      "fixed": {"entity_id": "obj3d_240", "feature": "axis_v", "offset_m": [0, 0.009, 0.012]},
      "moving": {"entity_id": "obj3d_239", "feature": "axis_v", "offset_m": [0, -0.2286, 0]},
      "axis": [-1, 0, 0],
      "min_deg": 0, "max_deg": 90, "position_deg": 0
    },
    {"operation": "generate_motion_envelope", "rule_id": "worktop_hinge", "samples": 33}
  ]
}
```

This sign convention gives **0° horizontal deployed**, **90° vertically down
stowed**, about pivot [0.65,0.9261,0.901] metres. To stow, apply a new batch with
`{"operation":"set_travel_position","rule_id":"worktop_hinge","position_deg":90}`.
Use 0 to deploy. Existing review app `assembly_capacity_20261008/sCulpt Main Edit.app`
reads these native records and exposes the normal Measure/Move saved motion controls.
No installation is implied. Recompute offsets when the owning scene geometry changes.

`tests/test_layout_hinge_tool.py TOOL PRECURSOR NEW_PROOF_ROOT` verifies the actual
edge pivot at 0/15/30/45/60/75/90°, leaf corners against the conservative envelope,
native round-trip, envelope freshness, input/other-data preservation and failed
batch refusal. Constraints/Motion suites cover native hinge solver and dense
intermediate conservative coverage, including assembly scope.


## Update an existing inspection camera

`update_inspection_camera` reuses a bound stationary camera/path pair, including
when all sixteen path slots are occupied. Required fields: `camera_id`, `path_id`,
`eye_m`, `target_m` and `fov_deg` (10–150). Optional `name` renames both labels;
omitting it preserves their labels.

```json
{"schema":"line_drawing_layout_edit_v1","operations":[
  {"operation":"update_inspection_camera","camera_id":"camera_006",
   "path_id":"path_camera_006","name":"Exterior door review",
   "eye_m":[2.5,1.42,1.3],"target_m":[0.55,1.05,0.8],"fov_deg":65}
]}
```

The existing pair must have matching bidirectional bindings, camera role, no
light binding, an open stopped linear path and exactly two points separated by
at most 0.00001 metres. Native loading clears transient playing state. Moving
routes and missing/mismatched IDs refuse. It applies the same one-second,
near-stationary two-point look-at setup as `add_inspection_camera`, near/far clips
0.01/100 metres and zero roll. Pose keys are regenerated with the native
initializer so stale orientation/FOV cannot override the new frame. Camera/path
persistent IDs, counts and ordering stay fixed; pose-key IDs are regenerated.
All other cameras, paths and non-camera native state are preserved. The revision
remains create-only and failed batches publish no candidate.

## Assembly and object visibility

The selected assembly/object inspector provides **Hide**, **Show**, **Only** and
**Restore** for temporary inspection. Assembly operations include descendants.
These overrides use stable entity IDs, share the native rendering/picking filter,
and create no geometry/history edits. Restore clears these inspection overrides;
authored hidden flags remain respected. Existing saved visibility queries and
saved camera/path records can reference this same scene. Scene load clears the
temporary inspection overrides.

`set_visibility` is the persistent counterpart in `layout_edit_tool`:

```json
{"schema":"line_drawing_layout_edit_v1","operations":[{"operation":"set_visibility","entity_id":"assembly_001","visible":false}]}
```

It changes only native object visibility flags for the requested object or assembly
subtree, in one undoable transaction. Unknown IDs refuse without a partial edit.
This command does not change mesh data, transforms, constraints or review cameras.

## Coupled fixed-length folding support (Main Edit)

`attach_folding_brace` accepts `rule_id` for an existing angular travel rule and
`brace: {fixed_offset_m:[u,v,n], leaf_offset_m:[u,v,n], length_m:[L1,L2],
branch:1, parts:[nine persistent IDs]}`. Offsets are metres in each referenced
primitive's native frame. Branch is +1 or -1. Parts are, in order: fixed-to-elbow
link, elbow-to-leaf link, fixed plate, leaf plate, fixed pin envelope, elbow pin
envelope, leaf pin envelope, deployed stop envelope, and release handle envelope.
The two source meshes must be centred, with their invariant length along local X;
all nine meshes retain metre source bounds and exact physical scale. Support
members must be unique and belong to no other driving rule or rigid motion scope.

The elbow is the circle intersection derived from the current anchors and hinge
axis. The selected branch stays fixed, including the straight deployed pose.
Analytic extrema check reachability over the entire declared hinge range before
publication. Slider, Min, Max, Reset and `set_travel_position` use the same native
transaction; the support receives rigid transforms, with no per-frame mesh
regeneration or stretching. Invalid geometry, unreachable range and conflicting
locked members reject atomically. Native save/load and undo retain the coupling.

`set_parent` accepts `entity_id` and `parent_id` for existing native entities.
`add_mesh_group` can use `existing_assembly` instead of creating a new group;
this route requires zero group rotation and preserves existing members. This
allows the support to reuse a cabinet group at the existing 64-assembly limit.

Native documents now save schema 26; the reader accepts earlier schemas. Older
builds must refuse these new documents, rather than silently remove the coupling.
The old primitive worktop envelope is stale once support is attached. Native
primitive-only envelope generation refuses the coupled mechanism; use explicit
conservative bounds for the leaf and all support parts, including thickness.

Legacy stop and release parts remain illustrative envelopes. Schema 26 adds
optional `compactMount` and `manualLock` records. Compact mounts declare two
`link_lateral_m` offsets, three optional `pin_lateral_m` offsets (zero by default),
and `fixed_plate_offset_m` / `leaf_plate_offset_m` triples, all in metres. Parts
7 and 8 become the elbow-mounted guide and axially withdrawn key.

`set_brace_lock` accepts `rule_id` and boolean `engaged`. The native motion pane
shows support status and Release/Engage support lock. Folding while engaged
refuses atomically with Release guidance; engagement requires the fully deployed
aligned pose. Reset changes travel position while preserving lock state. Native
save/load and undo preserve both pose and lock. This is an explicit manual
kinematic interlock; it does not simulate gravity, friction, contact response,
purchased hardware or load capacity. `detach_folding_brace` removes only the
selected derived coupling; `set_hinge_fixed_offset` requires it detached.
`delete_object` obeys native locked/driven and managed-furniture guards. A moved mechanism fixture verifies transformed parent frames; moving
a full cabinet still respects its existing external furniture contacts and can
be refused. Physical overlap warnings require review before hardware selection.

## Main Edit articulated fronts and inspection collections

The Main Edit development parser supports 32 saved visibility presets and sixteen
camera/path records, preserving earlier documents and their existing records.
Schema 27 adds optional finite telescoping stays and closed-neighbour motion guards.
These are geometric mechanisms; no spring force, damping, load or hold-open model
is implied.

`attach_telescoping_stay` and `update_telescoping_stay` take `rule_id` and `stay`.
The stay contains `fixed_offset_m`, `leaf_offset_m`, `length_m` (body and rod),
`insertion_m` (positive minimum/maximum), `fixed_plate_offset_m`,
`leaf_plate_offset_m`, and six `parts`: body, rod, fixed eye, leaf eye, fixed plate,
leaf plate. All are rigid imported meshes with unit physical scale. Whole-range
anchor extrema validate insertion before publishing a change. The native solver
updates rigid transforms and shares the ordinary Min/Max/Reset, history and
save/load paths. Motion-envelope generation still refuses unsupported mesh scopes.

`set_motion_interlock` takes `rule_id` and `closed_neighbor`. Either motion may
leave home only while the other remains at home. The guard is stored as
`closedNeighbor` on one rule and enforced symmetrically on both native motions.
A conflicting move is refused before publication; malformed references and
simultaneously open saved states are rejected. This is a declared service access
restriction, not automatic collision detection. `set_travel_limits` takes the
native `min_deg`/`max_deg` or `min_m`/`max_m`, preserving home/current position.

`create_assembly` returns a new native ID and accepts a `name` and optional
`parent_id`. `set_properties` merges text properties through the native metadata
transaction. A covering requires both `cover_group` (`doors/area/item`,
`drawers/area/item`, or `access/area/item`) and `cover_part` (`front` or `body`).
The Coverings tree groups these explicit memberships by kind, area and item.
Carcasses and hardware are included only if explicitly tagged. Showing a parent
preserves a separately hidden child. **Restore covers** resets covering inspection
without restoring hidden guides or unrelated visibility choices.

`save_covering_preset` takes `view_id`, `name` and `collection_id` (for example
`@coverings.doors.kitchen.fridge`). `save_guide_preset` takes `view_id`, `name` and
`category`: `all`, `motion`, `walkways`, `keepouts`, `service`, `corridors`,
`guides` or `annotations`. Nonphysical categories share the header Guide tree;
physical hardware envelopes retain their physical membership.

For agent inspection, `layout_edit_tool visibility INPUT COMMAND.json` accepts
schema `line_drawing_visibility_v1` and `hide`, `show`, `only`, `inspect` or `reset`
operations. Collection actions use `collection_id`. The report contains exact
memberships and hidden/shown/mixed states (0/1/2). These temporary actions never
write the input document or create geometry history.

## Pass 1 transient visibility inspection

`layout_edit_tool visibility INPUT.layout.json COMMAND.json` accepts
`line_drawing_visibility_v1` commands addressing covering collections, scene
collections or native entity IDs. `show`, `hide`, `only`, `reveal`, and `inspect`
reuse the native visibility state; returned collections include `visibility_reason`.
`reset` restores coverings inspection state. No document is written.


`configure_furniture_rail` uses `assembly_id`, `object_id`, `offset_m` and `span_m`
arrays in native assembly coordinates. Only an existing RUN_RAIL role can be edited.
Offsets/spans pass the unit's atomic regeneration and lock/validation checks.
A zero run span retains the legacy full run; a positive span must fit inside the
unit and its run offset. Identity, roles and other members remain fixed.

`set_properties` also accepts an optional `name` display label. The native metadata transaction preserves geometry and persistent IDs; missing entities and empty, oversized or non-text labels are refused without publishing output. This permits correcting handed hardware labels while retaining the same component identity.

### Paired mechanisms on one hinge

`attach_folding_brace` may include `paired_brace`, using the same nine-part
brace payload as `brace`. Both mechanisms share the existing hinge and move
atomically. The optional second mechanism must use distinct parts, has its own
pin centers and link lengths, and cannot carry a separate manual lock. The
primary support lock controls the hinge. Detaching clears both mechanisms.

Paired documents save as schema 30. Older documents retain schema 28/29 when
no pair is present. Older apps refuse schema 30 rather than silently dropping
the second mechanism. Full-range reachability, rigid SI mesh dimensions,
source poses and member ownership are validated for each mechanism.
This is kinematic behavior; supplier internal geometry, friction, fastening
and load/strength qualification are separate.
