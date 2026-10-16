# Importing dimensioned STL devices

Implemented in the persistent Main Edit development lane, 2026-10-08. The stable
sCulpt release is a separate artifact.

In Scene mode, open the File pane's STL browser and choose the source directory.
Before adding a file, set these two mouse controls above the file list:

- **Source: m/cm/mm/in/ft** cycles the coordinate unit. STL contains no unit
  declaration; choose the unit used by the asset producer. A fresh app session
  starts at **m** for compatibility with earlier metre-authored files. These
  choices are session settings, not guesses from filename or size.
- **Exact size** is the default. It converts source coordinates to physical
  metres and accounts for the document's metres-per-world-unit scale. It never
  resizes a device to fit the scene. **Fit to scene** is an explicit visual-model
  option; its object dimensions are unsuitable as manufacturer dimensional proof.

Settings are locked during background import. Repeated adds use the selected
unit's runtime cache. Different units use separate asset outputs; changing units
and adding again creates another instance without resizing an existing instance.
Cache readback checks the authoring source path and unit/scale. A filename that
collides with another source or unit is refused rather than replacing it; choose
a separate object asset root. This is a conservative cache guard, not immutable
content-addressed source revision history.

The importer preserves source axes and vertex origin, placing the instance at
world origin with zero rotation. Exact placement refuses an origin outside the
scene's enabled bounds instead of moving it. Select the imported object and read
**Local axes X/Y/Z** in its inspector to check its dimensions in the current display
unit. These spans follow the mesh's local axes and instance scale; they are not a
rotated world bounding box. Keep the source orientation until its intended mounting
axes are established, then use ordinary object transforms and Undo/Redo.

The runtime mesh is measured in metres. Its authoring sidecar retains the source
unit, source-to-asset conversion and source-axis policy. Native scene Save/Reload
retains the instance and its transform; scene export packages the runtime mesh
through the existing dependency-verification pipeline. Import parsing completes
before an authoring sidecar is saved. A failed file write can still leave partial
cache output; an import failure is not a successful device addition.

For example, a producer's millimetre mesh with X/Y/Z spans 91/67/195 should read
91/67/195 mm after **Source: mm / Exact size**, including in a document whose
coordinates represent centimetres. Axis order must be established from the actual
asset; an envelope drawing alone does not establish mesh orientation.

## Boundaries

This slice imports ASCII/binary STL using the existing shared mesh compiler. It
does not read or tessellate STEP, add device ports, generate cables, certify exact
fit, select product variants, or establish manufacturer reuse permission. The
Orion manufacturer STEP remains a source input requiring a separate CAD conversion
backend and geometry qualification. Protected van scenes are not changed by the
program tests. Human visual review and actual-device acceptance remain separate.
