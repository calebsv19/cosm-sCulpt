# Layout Module

The layout module stores the floor-plan geometry as anchors connected by walls, supports persistence, and prepares data for rendering and selection.

## Files
- `layout.h` / `layout.c` — define the `Layout`, `Anchor`, and `Wall` structures and implement the management API. Handles anchor deduplication, wall creation, removal, deletion marking, auto-prune logic driven by the editor's delete mode, and compaction of deleted entities. Mutating functions now call `Global_FlagLayoutChanged` so dirty state is tracked centrally.
- `layout_origin.h` / `layout_origin.c` — recentre the layout by shifting all anchors so a chosen anchor becomes the world origin and updating the grid offsets to keep it in view (raising both layout and grid dirty flags).
- `layout_json.h` / `layout_json.c` — load and save layouts using cJSON (`external/cjson`). Persists anchor positions and persistence flags, rebuilds walls with safety checks, and logs failures via SDL. In addition to file IO, `Layout_SaveToString` / `Layout_LoadFromString` expose in-memory snapshots for the undo/redo system. Successful loads mark the layout as dirty so hitboxes rebuild on the next frame.
- `render_layout.h` / `render_layout.c` — draw walls and anchors. Highlights the selected wall or anchor, colours persistent anchors, and uses the grid to convert world space to screen coordinates.
- `hitbox_system.h` / `hitbox_system.c` — rebuilds screen-space rectangles covering walls and anchors so the input system can do hit testing. Deleted walls/anchors are skipped, keeping intermediate states (pre-compaction) safe.

## Concepts
- Anchors track their connected wall indices; during compaction the module rewrites those indices so references stay valid after deletions.
- Dirty tracking: `Global_FlagLayoutChanged` marks the layout for compaction, which now runs on-demand (via `Global_TickSystems` or the input/render hot path) instead of every frame.
- `Anchor.isPersistent` prevents auto-prune from discarding a disconnected anchor, letting you pin important points while cleaning stray nodes.

## Related Modules
- `Editor_ClickAt` and delete actions rely on `Layout_AddWall`, `Layout_RemoveWall`, and `Layout_RemoveAnchor`.
- `Global_RebuildHitboxesIfDirty` schedules compaction and hitbox regeneration after layout mutations so selection stays accurate.
- UI buttons for saving/loading and origin reset call into this module's JSON and origin helpers (raising the appropriate dirty flags along the way).
