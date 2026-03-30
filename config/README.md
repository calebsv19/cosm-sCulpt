# Config

This folder holds persisted data that the editor loads on startup and writes when you save from the UI.

## Files
- `layout_config.json` — default floor-plan description. Contains:
  - `file`: metadata describing the `schemaVersion`, generator tag, and `gridSize`.
  - `anchors`: world-space positions (`x`, `y`, `z`) with a `persistent` flag that prevents auto-prune deletion, the anchor `type` (`corner` or `curve`), bezier handle linkage (`handlesLinked`), handle basis axis (`handleAxis`), and the polar handle data (`handleInLength`, `handleInAngleDeg`, `handleOutLength`, `handleOutAngleDeg`). If `z` is omitted (legacy 2D or hand-edited JSON), it defaults to `0`.
  - `walls`: zero-based index pairs `a`/`b` referencing the anchors array.
- `.DS_Store` — macOS metadata; ignored by the build.
- `space_mode.txt` — startup mode seed for dimension behavior (`2d` or `3d`).

## Usage
- `Layout_LoadFromFile` rebuilds anchors and walls from `layout_config.json` and restores the `persistent` flags. Files saved with a future `schemaVersion` are rejected to avoid corrupting the current runtime.
- The scene contract is additive: unknown root/file/anchor/wall fields are ignored by the loader so future metadata does not break current builds.
- `Layout_SaveToFile` rewrites the file when you click "Save JSON" in the UI.
- `Global_LoadSpaceMode` reads `space_mode.txt` at startup and applies mode constraints.
- `Global_SaveSpaceMode` updates `space_mode.txt` when mode is toggled through keyboard/UI.
- Ensure `walls` only reference valid anchors if you hand-edit the JSON—the loader skips invalid indices. When editing curved anchors manually remember to keep handle data consistent; linked handles are normalised automatically on load/save.
