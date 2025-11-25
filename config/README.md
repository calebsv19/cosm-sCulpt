# Config

This folder holds persisted data that the editor loads on startup and writes when you save from the UI.

## Files
- `layout_config.json` — default floor-plan description. Contains:
  - `file`: metadata describing the `schemaVersion`, generator tag, and `gridSize`.
  - `anchors`: world-space positions (`x`, `y`) with a `persistent` flag that prevents auto-prune deletion, the anchor `type` (`corner` or `curve`), bezier handle linkage (`handlesLinked`), and the polar handle data (`handleInLength`, `handleInAngleDeg`, `handleOutLength`, `handleOutAngleDeg`). Missing fields default to straight segments when older JSON is loaded.
  - `walls`: zero-based index pairs `a`/`b` referencing the anchors array.
- `.DS_Store` — macOS metadata; ignored by the build.

## Usage
- `Layout_LoadFromFile` rebuilds anchors and walls from `layout_config.json` and restores the `persistent` flags. Files saved with a future `schemaVersion` are rejected to avoid corrupting the current runtime.
- `Layout_SaveToFile` rewrites the file when you click "Save JSON" in the UI.
- Ensure `walls` only reference valid anchors if you hand-edit the JSON—the loader skips invalid indices. When editing curved anchors manually remember to keep handle data consistent; linked handles are normalised automatically on load/save.
