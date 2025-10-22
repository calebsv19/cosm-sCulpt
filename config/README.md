# Config

This folder holds persisted data that the editor loads on startup and writes when you save from the UI.

## Files
- `layout_config.json` — default floor-plan description. Contains:
  - `anchors`: world-space positions (`x`, `y`) with a `persistent` flag that prevents auto-prune deletion.
  - `walls`: zero-based index pairs `a`/`b` referencing the anchors array.
- `.DS_Store` — macOS metadata; ignored by the build.

## Usage
- `Layout_LoadFromFile` rebuilds anchors and walls from `layout_config.json` and restores the `persistent` flags.
- `Layout_SaveToFile` rewrites the file when you click "Save JSON" in the UI.
- Ensure `walls` only reference valid anchors if you hand-edit the JSON—the loader skips invalid indices.
