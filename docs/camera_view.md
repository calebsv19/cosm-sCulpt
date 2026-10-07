# C1–C3 camera navigation, path poses and directional inspection

Implemented in persistent Main Edit on 2026-10-06. The full F4 van is the
acceptance scene; camera navigation is generic and does not rebuild furniture.

## Human controls

Open **View > Camera / section**. **Explore** starts a temporary perspective
camera at world X=0, Y=0, Z=1.6 meters, looking toward -Y. This is a convenience
starting pose, not an inferred vehicle origin or a walking/collision simulation.
**< / >** selects an existing scene camera; **Enter** looks through its evaluated
pose, including the current normalized position on a bound camera path. Enter
pauses that path and leaves its control points and traversal position intact.

Move the pointer into the canvas to navigate. Hold W/S for forward/back along
camera heading on the world XY floor, A/D for strafe, Q/E for world-Z down/up.
Movement is in meters per second, with diagonal normalization and bounded wake
delta. Hold **Alt/Option and move the mouse without clicking** to look horizontally
and vertically. **Alt+Shift** and horizontal mouse movement rolls the camera.
The **Alt: Look / Alt: Roll** buttons also choose the mouse behavior, and **Level**
clears roll. Mouse-look belongs to the canvas and stops at its boundary; there is
no pointer lock or OS cursor warp. Vertical look is limited to +/-89.5 degrees.

The six visible movement buttons provide 100 mm steps without learning shortcuts.
**Details** expands unit-aware X/Y/Z plus yaw, pitch, roll, FOV, speed, near and far
fields. Click a field, type, and press Enter; Escape cancels the field. XYZ accepts
explicit unit suffixes. FOV is vertical; aspect follows the solved canvas size,
including pane resizing. The default near distance is 10 mm and is editable.

**Save new** creates a stable-ID standalone camera. **Update** commits changes to
a standalone camera, with one undo entry. Bound path cameras cannot be updated
through this control; Save new preserves their authored path. Save the document
normally to persist the committed camera. Temporary navigation itself creates no
undo entries and does not dirty the document. Undo/redo reconciles the camera
selector and clears stale save notices while preserving the temporary observer pose.

**Exit**, Escape, or the top-bar Camera chip restores the previous orthographic
editor camera/grid and section state. Choosing another authoring tab exits
observer mode. A scene load or workspace change also exits it. Focus loss,
leaving the window/canvas, text entry and pane clicks clear held movement.
Camera mode uses depth-tested physical surfaces and rendered-surface selection;
it suppresses engineering cages, routes, measurement overlays and edit gizmos.
Standalone saved cameras have constant-screen-size symbols in the editor view.
Routing corridors and semantic service/keep-out/motion volumes are presentation
only: they never enter the opaque surface, depth, or surface-picking pass, even
when older scenes lack an explicit reserved-space role. They remain thin outlines
in ordinary engineering views and are omitted in the physical camera. Their
geometry, route references, queries and validation roles are unchanged.
The guide classification is app-local presentation policy (reuse-deferred);
existing core_scene metadata and core_mesh_preview raster composition remain
adopted without any shared API/version changes.

## C2 directional inspection

**Inspection: off / on** is a visible button in View > Camera / section, usable
from the ordinary 3D editor or a perspective camera. Turning it on chooses Material
preview. Off restores ordinary physical occlusion; this is a transient viewing
choice and creates no undo entry or document change. Scene load/workspace change
resets it to off. Wire/Solid/Material remain available independently in the editor.

Near-side wall and furniture units with an inward-facing direction become subdued
outlines; opposite-facing units stay depth-tested solid. Direction is deliberately
explicit: the viewer does not guess it from a display name, van side, bounds, or a
triangle normal. Unassigned objects, explicit Solid overrides and tangent views
stay solid. The free-standing bed stays solid in the full van example.

**Facing** expands a compact editor for the selected object. **Unit / Part** chooses
its furniture assembly or just the selected member; the target name is shown.
**+X/-X, +Y/-Y, +Z/-Z** declare the inward direction in that target's rigid local
frame. **Inherit** removes its override; **Solid** stops ancestor inheritance.
Each edit uses the existing validated metadata/undo transaction. Save the file
normally to persist it. If all eight metadata slots are occupied, the edit refuses
without dropping other fields; use a parent assembly instead.

The existing optional text property `inspection_facing` stores `solid` or one of
those six directions; missing/`inherit` follows the nearest declaring ancestor.
An invalid reserved value is rejected by native engineering validation. Assembly
frames supply world direction, so rotation and nesting are respected. All members
inherit one viewing-heading classification rather than switching independently
by their distance from the camera. Units stay solid up to 15 degrees beyond
side-on, giving a 210-degree solid span in angular cross-section;
dot(inward, view-forward) > sin(15 degrees), approximately 0.259, selects outline.
The threshold is fixed and independent of camera FOV; it adds no periodic work.
This is a directional inspection aid, not automatic proximity removal or triangle
back-face culling. The F4 acceptance fixture declares directions on wall-side
units and shell proxies; only metadata changed. Routes, contacts, motion ranges,
object IDs and dimensions are retained.

Native panels and imported meshes use separate near-side coverage through the
same clipping/rasterizer and existing `kit_viewport3d` outline composition. Reference shell and construction use two bounded outline groups with one reused
temporary target, so shell interiors cannot mask construction edges. Near
coverage contributes only subdued one-raster-pixel edges (88/255 alpha), brighter
when selected/hovered. Transparent interiors retain the far surface's pick owner;
visible near edges remain selectable. Edges behind a solid surface are omitted.
Guide volumes remain queryable and are omitted here, as in the physical camera. Inspection
suppresses engineering cages/manipulation overlays; turn it off to manipulate.
Exact sections/cutaways still apply and no view operation changes their geometry.

The extra coverage buffers are lazy, bounded to the existing reduced viewport
size and reused. View direction/mode or inherited assembly metadata invalidate
the cached surface once, then the normal final quality refinement settles back
to sleep. No new periodic wake or scene mutation is introduced.

Reuse decision: `core_scene` metadata, `core_math`, `core_time`, `core_mesh_preview`
and `kit_viewport3d` owner/depth outlines stay adopted. Direction inheritance and
near/far presentation are app-local purpose policy (reuse-deferred); no shared
API/version or minimum-adoption changes are needed.

## Projection and ownership

The app adapter adds an explicit PerspectiveView to SpaceViewContext; existing
orthographic FreeViewCamera, core_viewport3d orbit/pan and document geometry stay
unchanged. Projection and screen rays use one eye/forward/right/up/FOV basis.
Native and imported triangles clip against all six camera frustum planes before
projection, then use reciprocal-depth interpolation in the existing shared
color/depth/owner raster pass. Camera changes invalidate its cached projection;
held movement schedules 16 ms updates, while released stationary cameras return
to the demand loop after preview refinement settles.

Reuse: core_math arithmetic, core_units conversion, core_time/demand scheduling,
core_scene/core_scene_compile camera persistence/export, and kit_pane/kit_viewport3d
presentation remain adopted. The perspective session, input policy and projection
adapter are app-local (reuse-deferred for a new shared camera abstraction). No
shared library API/version or minimum-adoption contract changes were needed.
Standalone camera evaluation now works without a path, including existing scene
export; fixed camera roll uses a continuous Z-up heading basis near vertical.

## Evidence and limits

CameraView tests cover perspective/ray agreement, inverse-distance projection,
frustum clipping, native depth ordering and near-plane crossings, Alt mouse input
without a button, roll, focus loss, held-input scheduling, bounded movement,
unit-aware responsive fields, undo/redo, standalone camera JSON/export and path
sample/pause/editor restoration. Native camera-interior proof renders the full
130-object F4 van through the Vulkan viewport:

```
LINE_DRAWING_VISUAL_ARTIFACT=/tmp/camera-interior.bmp \
LINE_DRAWING_VISUAL_ARTIFACT_MODE=camera-interior \
build/toolchains/clang/bin/LineDrawing
```

C2 tests cover both van sides, inheritance, rotated declaring frames, Solid overrides,
view-only serialization, visible responsive controls, undo/redo, metadata roundtrip,
and two-sided raster/pick ownership. `camera-inspection-left` and
`camera-inspection-right` native artifact modes render the full F4 scene, verify
far-surface picking plus near construction-edge picking, and check that the
preview has no remaining scheduled refinement.

Renderer image/frame parity remains subsequent C4 work. Preview colors retain existing material heuristics;
this is a construction preview, not a photorealistic render or fit certification.


## C3 path poses (C3.1–C3.5, 2026-10-06)

**Path poses** expands the selected camera path in View > Camera / section.
Entering a camera view or selecting a camera path also exposes it. Choose a
**Point**, then **View** to look through that exact positional anchor. Bézier
handles remain curve controls and are not camera poses. **< Point / Point >**
reach the whole bounded point list; four rows are shown at a time. Buttons use
two columns when their measured text fits and one column in narrower panes;
the existing right-pane scrolling keeps the controls reachable.

Navigate and edit Details as usual, then **Apply** commits position, heading,
pitch, roll and vertical FOV to the viewed point in one Undo transaction. Moving
a Bézier anchor carries its adjacent handles. Near/far clipping remains owned
by the bound camera and is committed by Apply. Selection alone does not change
the observer. Apply refuses a different selected point or a source changed by
another edit/Undo; **Revert** reads the selected point again and discards the
navigation draft. Play, Scrub and View refuse to replace a modified draft until
it is applied or reverted. Navigation during playback pauses it and starts a
draft. Focus loss and exit pause playback.

**New path from view** creates a separate one-point linear camera path and bound
camera, preserving existing exterior paths. Navigate, then **Add after** inserts
the current pose after the selected point, with one Undo entry. **Point actions**
exposes positive **Leg seconds**, **Earlier / Later**, **Remove point**, and
**Open / Closed**. Pose IDs, orientation, FOV and outgoing duration travel with
the anchor when reordered. Automatic Bézier handles are recalculated by their
existing tangent policy; other handle offsets travel with their anchors. The
last point is retained. The existing capacity remains 16 linear points or six
complete cubic anchors (16 curve controls); capacity refusal leaves the path
unchanged. Keyed paths require explicit topology editing and do not use the old
curve-type cycle to discard their poses.

**Play / Pause** and the seconds **Scrub** control evaluate authored paths without
creating history or modifying the document. Position follows the existing sampled
arc-distance geometry inside each timed leg; orientation uses shortest-arc
quaternion interpolation, with linear FOV interpolation. Positive leg times also
support a stationary turn between coincident anchors. Once ends at the final pose;
Loop repeats. Closed paths use the existing straight closing leg. An open loop
can jump at its seam; C3 does not add a smoothing algorithm. A paused preview
returns to the demand loop after the existing mesh refinement settles.

The additive version-1 `cameraPoses` native payload is owned by the path. It
contains stable pose IDs, quaternion w/x/y/z orientation in the X-forward/Z-up
frame, vertical FOV and outgoing seconds; position stays in the existing anchor
array. Old files with no payload retain fixed/path-facing/look-at behavior and
receive keys only through an explicit pose/topology/time edit. Unsupported or
invalid pose payloads reject the load atomically. Playback state is transient
for keyed paths and reloads paused. Generic and light paths keep their existing
behavior.

Canonical export emits `camera_poses` and the evaluated camera frame/FOV. A copy
of the authored path records in
`extensions.line_drawing.camera_paths_v1` survives shared runtime compilation,
and the app importer uses it when top-level paths were compiled away. Static
export evaluates the authored normalized distance, not the transient preview
cursor. Full keys and durations remain in the extension. RayTracing consuming
that full frame/FOV and matching rendered images remains C4; C3 makes no renderer
parity claim.

`make test ARGS=CameraPath` covers legacy final-anchor direction/roll, quaternion
wrap and stationary turns, strict JSON refusal, Bézier handle ownership,
capacity refusal, full-van transactions/Undo, save/reopen, compiled export/import,
stale Apply targets, responsive controls, focus pause and idle scheduling.
Native `camera-path-idle`, `camera-path-narrow` and `camera-path-actions` artifact
modes read the existing 130-object van into an isolated proof session and do not
save it. The idle mode runs the real demand loop for 3.5 seconds and requires
at most two renders after its first settling second, with no remaining scheduled
camera or mesh refinement. These checks establish automated/native evidence;
human path-shot acceptance remains a separate review.
