# Menu Module

The Menu layer owns the top-level host screen that sits above the editor runtime.

## Files
- `line_drawing_host_menu.h` / `line_drawing_host_menu.c` — app-local host menu controller shell. The current version uses dedicated navigation, filter, content, detail, and footer regions so the host can grow into a durable browser surface instead of a flat launch list. The surviving shell now owns keyboard/mouse focus movement, split hover-vs-selection behavior, inline catalog filtering, selection/preview lookup, browse-header root-picker shortcuts, activation, and event routing.
- `line_drawing_host_menu_render.c` / `line_drawing_host_menu_render_common.c` / `line_drawing_host_menu_render_internal.h` / `line_drawing_host_menu_internal.h` — app-local host menu render lane extracted from the oversized shell. These files now own clipped text rendering, panel/chip helpers, section nav, row drawing, detail rendering, preview/detail space, and the top-level host-menu draw orchestration through the existing SDL/Vulkan-safe draw path, while the shared private header carries the shell/render contract. Path display basenames come from `src/Core/line_drawing_file_catalog.*`; row wording and text lifetime stay local to the render lane.
- `line_drawing_host_menu_layout.c` — app-local layout, hit-testing, and scroll-geometry lane extracted from the host-menu shell. It now owns region math, content/scrollbar hit resolution, selected-row visibility, and scrollbar drag/jump helpers so the public shell stays focused on state and routing.
- `line_drawing_recent_contexts.h` / `line_drawing_recent_contexts.c` — app-local recent-entry builder for Phase 2. It turns persisted recent layouts, scenes, input roots, and output roots into one mixed host-menu section with current-state badges and activation-ready metadata. It reuses the Core file-catalog basename helper while preserving host-menu-specific fallback labels and entry grouping.
- `line_drawing_scene_catalog.h` / `line_drawing_scene_catalog.c` — first app-local catalog backend for Phase 2. It now consumes the shared app-local catalog helper under `src/Core/` so the host menu and editor-local file browser enumerate the same `.json` layouts and valid authored scene directories (`scene_authoring.json` + `scene_runtime.json`), then tracks which layout/scene path is currently active for the host shell.
- `line_drawing_catalog_preview.h` / `line_drawing_catalog_preview.c` — app-local lightweight preview and metadata cache for Phase 2. It loads layouts or authored scenes into temporary `Layout` snapshots, derives primitive and bounds metadata, and projects a deterministic wireframe thumbnail for list rows and the detail pane.
- `line_drawing_root_browser.h` / `line_drawing_root_browser.c` — app-local root-context suggester for Phase 2. It owns the current root-context anchor plus ranked nearby child/sibling/cousin directory suggestions that look scene-like based on local JSON/scene-contract evidence, including representative preview targets for each suggestion row. Its ranking, preview description, and branch-search policy stay local, while reusable path/catalog primitives come from `src/Core/line_drawing_file_catalog.*`.

## Boundary
- This module owns host policy only:
  - initial app mode is `MENU`
  - `Enter` opens the selected action
  - `Esc` returns from editor runtime to the menu
  - `Ctrl/Cmd + M` also returns from editor runtime to the menu
- It does not own editor behavior, preview generation, or shared semantic contracts.
- It now owns the first top-level browsing lane for layouts/scenes, including inline filtering for names/paths, richer metadata rows, cached lightweight previews, a dedicated recents section, and a dedicated browse/root-context section with header-level root picking plus preview-backed nearby root suggestions that pivot into the active catalog lane on activation.
- Quick-action reopen targets now use independent remembered layout and scene paths instead of one shared current-session path, so reopening a scene does not overwrite the last JSON/layout quick action target.
