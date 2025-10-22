# Layout Module

The layout module stores the floor-plan geometry as anchors connected by walls, supports persistence, and prepares data for rendering and selection.

## Files
- `layout.h` / `layout.c` — define the `Layout`, `Anchor`, and `Wall` structures and implement the management API. Handles anchor deduplication, wall creation, removal, deletion marking, auto-prune logic driven by the editor's delete mode, and compaction of deleted entities.
- `layout_origin.h` / `layout_origin.c` — recentre the layout by shifting all anchors so a chosen anchor becomes the world origin and updating the grid offsets to keep it in view.
- `layout_json.h` / `layout_json.c` — load and save layouts using cJSON (`external/cjson`). Persists anchor positions and persistence flags, rebuilds walls with safety checks, and logs failures via SDL.
- `render_layout.h` / `render_layout.c` — draw walls and anchors. Highlights the selected wall or anchor, colours persistent anchors, and uses the grid to convert world space to screen coordinates.
- `hitbox_system.h` / `hitbox_system.c` — rebuilds screen-space rectangles covering walls and anchors so the input system can do hit testing. Maintains a fixed-size array of hitboxes and returns the top-most match for a given mouse position.

## Concepts
- Anchors track their connected wall indices; during compaction the module rewrites those indices so references stay valid after deletions.
- `Global_TickSystems` calls `Layout_CompactDeletedElements` each frame to physically remove anything marked `isDeleted` and optionally prune orphaned anchors when auto-prune mode is active.
- `Anchor.isPersistent` prevents auto-prune from discarding a disconnected anchor, letting you pin important points while cleaning stray nodes.

## Related Modules
- `Editor_ClickAt` and delete actions rely on `Layout_AddWall`, `Layout_RemoveWall`, and `Layout_RemoveAnchor`.
- `HitboxSystem_Rebuild` is triggered from the input handler before and after processing events so hit testing matches the latest layout.
- UI buttons for saving/loading and origin reset call into this module's JSON and origin helpers.
