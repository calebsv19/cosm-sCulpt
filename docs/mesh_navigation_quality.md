# Mesh navigation quality

Camera movement uses a display-only coarse mesh and a reduced raster target.
Orbit, pan, zoom, held navigation keys and camera playback restart or hold the
cheap navigation phase. Inspection, sections and selection obey the same motion
limit. Source vertices, physical units, transforms and saved visibility are not
changed by this policy.

The motion planner targets at most 32,000 triangles across the scene and at most
2,000 per mesh part. It keeps coherent 256-triangle surfaces across parts before
using bounds proxies where the minimum surface inventory exceeds the budget.
The raster target is at most about 180,000 pixels (integer rounding may add a
border). After 450 ms of quiet, the triangle target rises to 120,000; after
1.1 seconds it rises to 500,000. Selected and large projected parts receive
priority. The stable raster target remains about 890,000 pixels. Exact section
views retain their full-resolution stable raster. Refinement stops waking the
renderer after the final stage; input immediately restarts the motion phase.

The View panel shows Moving, Balanced or Detail, triangle count, frame cost,
loading count and hidden physical-part count. A roof can be hidden by the saved
scene or Visibility controls. The van full scene includes it; the separate
interior cutaway intentionally hides the roof and separate door reference.

Asset decoding and four coherent preview levels use the existing bounded single
worker and source-keyed prepared store. On macOS that worker uses Utility QoS.
Prepared preview packs use a versioned key derived from the source bytes and
LOD policy. The worker compares the requested key with the key in the checked
pack header before accepting its geometry. A pack filed under another mesh's
key triggers a fresh build from the requested source. Unreadable mesh data
keeps the proxy visible. Older v2 entries stay on disk as v3 entries are built
on demand.

Prepared native scenes avoid repeating STL import and mesh compilation. Loading
an arbitrary monolithic STL still requires importing/compiling it before it can
use this prepared scene path; navigation LOD does not make that initial import
free. Keep the full source as the master and use the prepared native scene for
routine inspection.

Regression coverage: VanUsability, LayoutMeshSolidPreview, AppearanceTree and
ViewportNavigationParity. The native navigation proof accepts
`LINE_DRAWING_NAVIGATION_PROOF_INSPECTION=1` and reports every motion frame's
budget plus final settled quality in `quality.json`.

Eight mesh preparation jobs can be outstanding across the worker and its
consumers. Polling transfers a job and its private LOD storage to the caller.
That result stays readable across cancellation, shutdown and pool restart, and
continues to occupy a slot until the caller frees it. Cancellation before poll
invalidates even a completed result. Shutdown joins the worker before freeing
jobs still on its list. Stats distinguish pending deliveries from reserved
result bytes, which also cover results held by consumers.

The preview cache moves accepted levels into its own resident budget and frees
the job. Its mesh getters lend pointers for immediate synchronous rasterization.
A cache reset, eviction or shutdown ends that borrow.
