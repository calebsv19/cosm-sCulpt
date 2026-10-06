# C1 perspective camera and interior navigation

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
undo entries and does not dirty the document.

**Exit**, Escape, or the top-bar Camera chip restores the previous orthographic
editor camera/grid and section state. Choosing another authoring tab exits
observer mode. A scene load or workspace change also exits it. Focus loss,
leaving the window/canvas, text entry and pane clicks clear held movement.
Camera mode uses depth-tested physical surfaces and rendered-surface selection;
it suppresses engineering cages, routes, measurement overlays and edit gizmos.
Standalone saved cameras have constant-screen-size symbols in the editor view.

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

C2 directional near-side outlining is not implemented by C1. Per-anchor camera
orientation editing, path-point preview controls and renderer image/frame parity
remain subsequent C3/C4 work. Preview colors retain existing material heuristics;
this is a construction preview, not a photorealistic render or fit certification.
