# Layout Module

The layout module stores the floor-plan geometry as anchors connected by walls, supports persistence, and prepares data for rendering and selection.

## Files
- `layout.h` / `layout.c` — define the `Layout`, `Anchor`, and `Wall` structures and implement the management API. Anchors now store bezier metadata (`AnchorType`, linked handle flag plus polar in/out handles) so curved segments can be rendered and edited. The API handles anchor deduplication, wall creation, removal, deletion marking, auto-prune logic driven by the editor's delete mode, bezier handle seeding when constraints allow, and compaction of deleted entities. Mutating functions call `Global_FlagLayoutChanged` so dirty state is tracked centrally.
- `layout_origin.h` / `layout_origin.c` — recentre the layout by shifting all anchors so a chosen anchor becomes the world origin and updating the grid offsets to keep it in view (raising both layout and grid dirty flags).
- `layout_json.h` / `layout_json.c` — load and save layouts using cJSON (`external/cjson`). Persists anchor positions, persistence flags, anchor type, linked-handle state, and polar handle data. Rebuilds walls with safety checks, emits metadata (`schemaVersion`, `generator`, `gridSize`), refuses to load future schema versions, and logs failures via SDL. In addition to file IO, `Layout_SaveToString` / `Layout_LoadFromString` expose in-memory snapshots for the undo/redo system. Successful loads mark the layout as dirty so hitboxes rebuild on the next frame.
- `render_layout.h` / `render_layout.c` — draw walls and anchors. Highlights selected/hovered anchors (including curved handle gizmos), renders bezier segments by sampling the cubic between endpoint handles, colours persistent anchors, and uses the grid to convert world space to screen coordinates.
- `hitbox_system.h` / `hitbox_system.c` — rebuilds screen-space rectangles covering walls, anchors, and bezier handles so the input system can do hit testing. Deleted walls/anchors are skipped, keeping intermediate states (pre-compaction) safe. Hitboxes are also generated for unconnected anchors so they remain selectable after deleting neighbouring walls.

## Concepts
- Anchors track their connected wall indices; during compaction the module rewrites those indices so references stay valid after deletions. For curved anchors, each wall uses the slot that corresponds to its direction (incoming or outgoing), ensuring bezier handles remain stable when the topology changes.
- Curved anchors require exactly two connected walls. `Layout_SetAnchorType` checks this constraint, seeds the handles from wall directions, and mirrors/locks handles when requested. The input layer can toggle linkage or edit handles directly, and linked handles remain symmetric thanks to `Anchor_ForceLinkedHandles`.
- Dirty tracking: `Global_FlagLayoutChanged` marks the layout for compaction, which now runs on-demand (via `Global_TickSystems` or the input/render hot path) instead of every frame. `Global_OnLayoutSaved/Loaded` snapshot the layout JSON to keep undo, dirty flags, and the active config path unified.
- `Anchor.isPersistent` prevents auto-prune from discarding a disconnected anchor, letting you pin important points while cleaning stray nodes.

## Related Modules
- `Editor_ClickAt`, multi-anchor dragging, and delete actions rely on `Layout_AddWall`, `Layout_RemoveWall`, `Layout_RemoveAnchor`, and `Layout_SetAnchorType`.
- `Global_RebuildHitboxesIfDirty` schedules compaction and hitbox regeneration after layout mutations so selection stays accurate.
- UI buttons for saving/loading and origin reset call into this module's JSON and origin helpers (raising the appropriate dirty flags along the way).
