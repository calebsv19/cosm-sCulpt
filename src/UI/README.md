# UI Module

The UI layer provides the editor-local button panel, quick file/scene pickers, and shared font management. The new top-level startup menu lives separately under `src/Menu/` so host routing does not get folded back into panel code.

- `ui_panel.h` / `panel/ui_panel.c` — define `UIButton` and `UIPanelState`, own pane-aware control metadata, construct the left/right grouped control surface, keep shared JSON/scene picker state plus scene-list scroll/hover state, keep object-model operation-history scroll/hover/drag state, and coordinate panel relayout against the current pane-host solve. The shell now keeps the left side wider for scene/file work while trimming the right side further through the pane-host target path so the viewport keeps more horizontal room.
- `ui_panel_shell.h` / `panel/ui_panel_shell.c` — own the Phase 1 editor shell state for the sidebars: left/right tab ids, tab button geometry, body-rect layout, and group visibility routing. Scene workspace tabs are `Scene` / `File` on the left and `View` / `Create` / `Object` / `Edit` on the right. Object workspace remaps the same shell ids to the CAD taxonomy: `Model` / `Assets` on the left and `View` / `Tools` / `Properties` / `Edit` on the right.
- `ui_panel_scene_layout.h` / `panel/ui_panel_scene_layout.c` — own explicit left-scene section layout. They reserve stable rects for the top summary card, scrollable scene browser, lower selection actions, and bottom bounds controls so the scene pane no longer infers list space by clipping around button bounds after the fact.
- `ui_panel_scene_summary.h` / `panel/ui_panel_scene_summary.c` — own the first scene-pane audit summary surface. It reserves a dedicated `Scene / Selection` card at the top of the left `Scene` tab and renders object counts, primitive-kind split, broader scene-graph counts, selected-object context, size, and lock-state summary above the list.
- `ui_panel_scene_list.h` / `panel/ui_panel_scene_list.c` — own the left-scene list and scene-local selection actions. It renders a scrollable stable row list of live plane/prism objects, tracks hover, handles wheel scrolling plus a visible draggable scrollbar, converts single row clicks into editor object selection, and uses same-row double-click to toggle one inline expanded detail lane at a time. It exposes `Clear Select` / `Delete Obj` helpers for the scene pane and now computes variable row heights so optional inline detail stays inside the owned list surface. The list now uses the explicit scene-pane section rect instead of deriving its height from lower button bounds.
- `ui_panel_file_summary.h` / `panel/ui_panel_file_summary.c` — own the first Phase 4 file/session cleanup slice. It reserves a dedicated summary card at the top of the left `File` tab and renders current layout, current scene, session input root, output root, dirty/clean state, browser status, and current mode-specific browser root above the file/root action buttons. In object workspace this same surface is the `Assets` action summary for object-asset save/load/new/export and root/path actions. Its text now clips through the card viewport instead of replacing overflow with `...`.
- `ui_panel_file_browser_internal.h`, `panel/ui_panel_file_browser_layout.c`, `panel/ui_panel_file_browser_state.c`, and `panel/ui_panel_file_browser_render.c` — own the first file-browser decomposition seam. They now carry persistent browser geometry/scroll math, active-versus-remembered row-state helpers, status/action-hint copy, and the actual pane-local browser renderer, so `ui_panel_load_menu.c` can keep scan/persist/load behavior as a thinner shell instead of mixing all browser math/render logic inline.
- `ui_panel_summary_surface.h` / `panel/ui_panel_summary_surface.c` — own shared pane-surface primitives for cards and list/browser headers: unclipped text, clip-rect text rendering, wrapped paragraph rendering for the elastic middle cards, accent-band cards, and divider drawing. The scene/file/view/create/object pane info surfaces plus the scene list and file browser now share one clipping/card-chrome path, and the right-pane middle work surfaces can reflow longer text instead of treating every sentence as a single clipped row.
- `ui_panel_visual_style.h` / `panel/ui_panel_visual_style.c` — own the top-level pane visual system: shared panel metrics, palette resolution, frame drawing, accent bands, label chips, divider helpers, interactive row rendering, shared scrollbar chrome, and the distinct workspace/list surface tone used by pane middle sections. This is the new line-drawing equivalent of the drawing program’s common panel render lane, so pane chrome, tabs, button framing, section containers, list rows, and browser rows can converge on one style contract instead of each renderer carrying its own constants.
- `ui_panel_right_controls.h` / `panel/ui_panel_right_controls.c` — own the shared right-pane control-grid lane. They define the compact row variants for `View`, `Create`/object-mode `Tools`, and `Object`/object-mode `Properties`, compute section heights from those row models, and lay out the lower button clusters as uniform equal-width rows instead of pane-local width heuristics inside `ui_panel.c`.
- `ui_panel_file_controls.h` / `panel/ui_panel_file_controls.c` — own the compact left-file control lane. They compute shorter file-pane button heights, compact 2-column row arrangements for `File / IO` and `Session Paths`, and keep file-pane control layout out of the already oversized `ui_panel.c` shell/router file.
- `ui_panel_file_layout.h` / `panel/ui_panel_file_layout.c` — own explicit left-file section layout. They now follow the same stronger ownership pattern as the scene pane: compact summary at the top, elastic browser in the middle, and anchored compact control groups at the bottom, instead of treating the browser as leftover space below stacked controls.
- `ui_panel_view_layout.h` / `panel/ui_panel_view_layout.c` — own explicit right-view section layout. They reserve a compact top context card, an elastic middle viewport workspace, and bottom-anchored `View` / `Modes` controls so the pane can grow into a fuller environment-management surface without unstable button drift.
- `ui_panel_overlay_render.h`, `overlay/ui_panel_overlay_render.c`, `overlay/ui_panel_overlay_dialog_common.c`, `overlay/ui_panel_overlay_dialog_file.c`, and `overlay/ui_panel_overlay_dialog_edit.c` — own the overlay dialog lane. The public overlay shell is now intentionally thin, while shared text helpers plus grouped file-dialog and edit-dialog renderers keep modal rendering out of one oversized translation unit. The stale overlay-owned legacy load-menu renderer has been removed from this path so browser ownership stays with the file-pane lane.
- `ui_panel_view_summary.h` / `panel/ui_panel_view_summary.c` — render the right-view context and viewport-workspace surfaces against the owned view-pane rects. The `View` tab shows live mode, zoom, plane, delete-state, and selection context above the lower control stack. In object workspace it reads as `View Actions`, reserving this pane for navigation, zoom, view-plane, display, and edit-mode controls rather than object-model mutation.
- `ui_panel_create_layout.h` / `panel/ui_panel_create_layout.c` — own explicit right-create section layout. They reserve a compact top context card, an elastic middle authoring workspace, and bottom-anchored `Primitives` / `Construction` controls so the `Create` tab can grow richer without shoving commit actions around. In object workspace this same rect contract is the `Tools` tab: existing face/sketch/extrude controls stay wired while the middle surface reports active tool state.
- `ui_panel_create_summary.h` / `panel/ui_panel_create_summary.c` — render the right-create context and authoring workspace surfaces against the owned create-pane rects. The scene `Create` tab shows active space/plane context, grid/unit state, and staged plane/prism readiness. In object workspace, the surface now reads as `Tools`, with a static `Command Actions` card for target/sketch/solid command groups and an `Active Command` card for current ready state, target, sketch, selected operation, and live extrude-depth/step state.
- `ui_panel_edit_layout.h` / `panel/ui_panel_edit_layout.c` and `ui_panel_edit_summary.h` / `panel/ui_panel_edit_summary.c` — own the right `Edit` tab in object workspace. The tab exposes explicit Body / Face / Edge / Vertex selection modes, summarizes the active authoring target and topology counts, and reserves the lower selection-mode group separately from the existing `Tools` command lane and `Properties` inspector lane.
- `ui_panel_object_layout.h` / `panel/ui_panel_object_layout.c` — own explicit right-object section layout. They reserve a compact top identity card, a dedicated inspector/details card, and separate rects for object actions, dimension edits, gizmo mode, and transform edits so the `Object`/object-mode `Properties` tab no longer reads like generic right-pane overflow.
- `ui_panel_object_inspector.h` / `panel/ui_panel_object_inspector.c` — render the right-object inspector surface against the owned object-pane rects. The scene `Object` tab and object-mode `Properties` tab now show a compact selection summary plus a fuller properties/details card with identity, transform, primitive, and state context before the object-local edit sections; in object mode, selected operations, sketches, bodies, and selected faces also surface their stable `FaceID` values and face-ref status beside the primitive face compatibility label before the persistent body dimension, gizmo, and transform edit controls. Command tools stay in the `Tools` pane.
- `ui_panel_object_workspace_summary.h` / `panel/ui_panel_object_workspace_summary.c` — render and hit-test the object workspace `Model` pane. It uses the attached `ObjectAuthoring` document for asset status, body/sketch/operation counts, selected body/face context, selectable body/sketch/operation rows that update the authoring selection contract, and a clipped scrollable operation-history lane with wheel, hover, and draggable-scrollbar handling. Its object-mode role is model selection/navigation, not asset I/O or command launching.
- `render_ui_panel.h` / `panel/render_ui_panel.c` — draw pane-scoped buttons/section chrome using SDL2 renderer primitives and SDL_ttf fonts. `DrawButton` handles background, border, hovered-state accent, and centred text; the shell renderer also draws the tab row, pane-surface framing, section-title chips, and the left-side scene list. Lower group chrome now prefers the pane-owned section rects when a tab has explicit layout ownership, so the section frames match the layout contract instead of inferring their bounds only from button extents. File/session and right-side state summary routes delegate to dedicated tab-specific summary modules.
- `input_ui_panel.h` / `panel/input_ui_panel.c` — detect clicks inside button bounds before layout editing occurs. Executes actions such as saving JSON (modal filename prompt), opening native folder pickers for JSON and scene roots, root-path edits/folder picks, resetting the origin, zooming, toggling delete mode, pinning the selected anchor, cycling object center-gizmo mode (`Move` / `Rotate` / `Size`), toggling object edit selection mode (`Body` / `Face` / `Edge` / `Vertex`), toggling `SpaceMode` (`2D`/`3D`), and fitting scene bounds to the selected plane/prism, and raises the appropriate dirty flags so hitboxes/layout stay in sync.
- `panel/ui_panel_controls.c` — declare and seed the grouped button/control set used by the pane-scoped right/left sidebars.
- `panel/ui_panel_dialog_logic.c` — typed dialog apply/cancel logic for dimensions, bounds, offsets, and selected-object transform editing.
- `panel/ui_panel_file_ops.c` — save/load/export and root-path file-operation helpers. It owns the native JSON/scene root pickers and `Export Shape` / `Export Scene`; scene export writes `scene_authoring.json` plus compiled `scene_runtime.json`, and scene-loaded sessions re-export back to the same authoring directory. Session input-root edits now stay explicitly separate from the lower file-browser mode roots and only rebuild the browser when the browser is actually following the same session root.
- `panel/ui_panel_input.c` — panel-local keyboard/text-entry helpers layered under the UI panel state machine.
- `panel/ui_panel_load_menu.c` — shared JSON/scene browser helpers plus panel mouse-hover tracking. It scans the active input root for JSON files or valid scene directories, drives the persistent left-file browser modes, restores the last browser mode from ignored runtime state, tracks hover/active rows, manages clipped scroll state, rejects empty candidate roots before mutating the current input root, forwards scene-list hover state, and turns `+Plane` / `+Prism` button hover into editor primitive placement preview state in 3D mode. The browser header now reports mode plus entry count, and rows/footer clip through the viewport instead of manually ellipsizing overflow.
- `overlay/ui_panel_overlay_render.c` — draw panel overlays such as modal dialog chrome above the pane-clipped base panel render path. JSON/scene browsing is now handled by the owned `File` pane browser rather than this overlay lane.
- `workspace_authoring/` — local host adapter for the shared Workspace Authoring layer. It stores authoring state on `GlobalState`, uses `kit_workspace_authoring` for entry-chord/trigger semantics, shared overlay button geometry/hit testing, and the shared full-screen Font/Theme panel model. It draws the S2 active-only pane overlay by adapting `LineDrawingPaneHost` leaves/module bindings into SDL/Vulkan-safe text and rectangle calls, draws the S3 Font/Theme overlay with host-owned runtime preview calls, and commits S4 accepted preferences only from the Apply path.
- `topbar/line_drawing_editor_topbar.h` / `topbar/line_drawing_editor_topbar.c` — own the production top-level menu/status bar. It renders one coordinated top-pane surface with the `Scene` / `Object` workspace switch, clipped selection context, file/dirty state, mode, view, plane, construction-plane, bounds, gizmo, and undo/redo groups so the top lane no longer stacks independent overlay renderers. `Mode`, `View`, `Bounds`, and `Gizmo` are active status chips: `Mode` mirrors `M`, `View` mirrors `V` and can enter 3D/free-view from 2D, `Bounds` toggles scene bounds through the panel bounds helper, and `Gizmo` mirrors `X`; `Plane` and `CP` remain readouts until their picker/stepper workflow is explicit.
- `info_overlay.h` / `info_overlay.c` — retain the older detailed diagnostic status formatter for future debug use. Normal runtime top-pane rendering now flows through the editor topbar.
- `font_manager.h` / `font_manager.c` — initialise SDL_ttf, load font resources from `include/fonts`, expose typed getters, bridge shared font preset selection into runtime reloads, persist accepted font preset/text-size step values under ignored `data/runtime/`, and release fonts on shutdown.

## Workflow
- The mouse handler routes by pane first, then calls `UIPanel_HandleClick`; returning `true` stops the editor from treating the click as a layout action. Motion events are still forwarded to the panel for overlay hover feedback and left-scene row hover, and wheel events now let the scene list consume scroll before viewport zoom runs.
- Rendering runs `LineDrawingEditorTopbar_Render` for the top-pane menu/status bar, then the unified `Render_UIPanel(...)` shell pass, `UIPanel_RenderOverlays` for the clipped JSON/scene picker plus save/root modals, and the active-only Workspace Authoring overlay. The current shell keeps the existing actions but scopes them by tab:
  - left `Scene`: scene/selection summary card, expandable authored-object browser, scene-local clear/delete actions, and scene-bounds controls
  - left `File`: file/session summary, save/load/export/root-path controls, and a persistent JSON/scene browser
  - right `View`: live view/editing summary plus origin/zoom/mode controls
  - right `Create`: live construction summary plus primitive creation and construction-plane controls
  - right `Object`: selected-object inspector plus prism dimensions, gizmo mode, and transform controls
  - right `Edit`: object-mode Body / Face / Edge / Vertex selection-mode control and live topology target summary
- In object workspace the same shell is intentionally re-labeled around CAD
  authoring roles:
  - left `Model`: object-asset status plus selectable body/sketch rows and a
    clipped scrollable operation-history lane for model navigation. Selected
    face, sketch, and operation rows now expose stable `FaceID` diagnostics and
    face-ref status beside the primitive face label adapter
  - left `Assets`: object asset save/load/new/export controls plus the asset
    browser/path controls
  - right `Tools`: face selection, sketch selection/drawing, and extrude
    actions with static command actions, active-command state surfaces, stable
    selected-target `FaceID` diagnostics, and first live extrude-depth
    parameter controls
  - right `Properties`: selected operation/sketch/body/face/object details
    plus persistent dimensions, gizmo, and transform controls
  - right `View`: viewport navigation, zoom, view-plane, and editing-mode
    controls
- The shell polish pass now adds stronger visual hierarchy without changing the
  control map:
  - pane surfaces have explicit framed chrome instead of flat sidebars
  - active tabs and hovered buttons expose clearer accents
  - summary cards use top accent bands and internal divider lines
  - the scene list renders inside a dedicated framed surface with stronger
    selected/hover row accents
  - the scene pane now uses more of its height for owned context through a top
    summary card and lower selection-action strip instead of treating nearly
    the whole tab as one passive list well
  - the scene pane now has explicit stable section ownership for summary, list,
    selection actions, and bounds controls, which removes the earlier fragile
    “list clipped above buttons” layout path
  - scene rows now use selection-first interaction:
    - single-click selects only
    - double-click toggles the inline detail state
  - one scene object at a time can now expand inline inside the list when
    requested, so top-level object management stays inside the scene pane
    rather than pushing everything into separate summary surfaces
  - longer scene-object lists now expose a visible draggable scrollbar instead
    of wheel-only navigation
  - the scene-list scroll lane now renders through an explicit viewport clip
    rect, so expanded rows cannot bleed into the summary, selection, or bounds
    sections, nested text respects the same bottom-edge mask as row fills, and
    the scrollbar can stay slimmer without content overlap
  - the file pane now uses the same owned-section approach instead of the old
    transient picker overlay:
    - `Load JSON` and `Load Scene` switch the lower file-pane browser between
      persistent modes
    - the browser keeps its list visible across ordinary file-pane actions
    - browser mode persists in ignored runtime state so the pane can restore
      whether it was last in JSON or scene mode
    - JSON and scene browser roots also persist independently, so the two modes
      do not have to share one browsing root
    - startup now follows that persisted browser mode so the editor session
      can reopen the last relevant layout/scene before the host menu hands off
      to the editor again
    - the restore path then rebuilds the browser from the real active session
      state so row highlighting stays derived from loaded content
    - the browser now also persists remembered last-entry paths for JSON and
      scene mode separately, and falls back to those remembered entries when no
      active loaded session path matches the current browser root
    - single-click on `Load JSON` / `Load Scene` switches the browser mode;
      double-click opens the corresponding folder picker and updates that
      mode’s saved root
    - the lower helper copy now explicitly states that `Session Paths` edits
      the live session root only; it does not overwrite the JSON/scene
      mode-specific browsing roots unless the user uses the mode-specific
      picker flow
    - remembered-entry persistence now lives in the shared layout/scene load
      path instead of only the browser row-click helper, so direct loads and
      startup restore keep browser highlight state coherent
    - direct layout/scene loads now refresh the browser immediately, so the
      highlighted row updates as soon as session state changes
    - plain browser-mode switching uses the per-mode saved root without
      clobbering live session root/path metadata
    - actual browser-row loads still re-establish the live input root from the
      active browser root before loading
    - if the current input root no longer contains either the active session
      file or the remembered entry, the browser now drops to no highlighted row
      instead of implying stale selection state
    - the `File / Session` summary now includes a browser-status line so the
      pane distinguishes active-session rows, remembered rows, empty-mode
      states, and roots with no entries
    - the summary now also renders a state-aware action-hint line, and the
      browser footer reuses that same hint contract so active-session rows,
      remembered fallback rows, and unmatched roots each explain why
      `Use Session` or `Clear Last` is the relevant recovery action
    - the browser rows now also carry an at-a-glance visual distinction:
      - `LIVE` chip for a true active-session row
      - `LAST` chip plus a warmer accent for a remembered fallback row
    - the `Session Paths` block is now explicitly session-scoped instead of
      implying that it edits the same root as the lower JSON/scene browser
    - the summary now exposes a separate `Browse In` line so the saved
      mode-specific browser root stays visible alongside the live session input
      root
    - the file pane now uses a denser UI allocation:
      - the top summary card is shorter and uses fewer text rows
      - the persistent browser owns the elastic middle section
      - `File / IO` and `Session Paths` are compact anchored bottom groups
      - both bottom groups now use 2-column button rows where appropriate
    - file-pane splitter drag now gets first chance before the persistent file
      browser captures clicks, so the left pane remains resizable while the
      `File` tab is active
    - browser refresh scrolls the active/remembered row into view so deeper
      lists behave more like the top-level host menu lists
    - `File / IO` now also includes explicit browser-state quick actions:
      - `Use Session` retargets the browser to the best working-session row
        for the active mode, preferring the still-existing loaded path and
        otherwise falling back to the most recent layout/scene history entry
      - session input-root edits now preserve that loaded layout identity
        instead of clobbering it back to the default `layout_config.json`
        path for the new root, so `Use Session` returns to a true active row
        after ordinary session-root changes
      - `Clear Last` removes the remembered fallback entry for the active mode
        so the browser can drop to no highlighted row without a root change
    - invalid or empty candidate roots are rejected before replacing the
      current input root
    - the browser header now avoids re-reporting the browse root above the
      entry list; that root stays in the summary card instead
    - active browser rows and active file/scene tabs now use subtler blended
      fills so bright presets keep readable contrast
    - compact file-pane button labels now clip to their button bounds instead
      of replacing overflow with `...`
  Pane mode overlays the program; Font/Theme mode fills the viewport and previews runtime font/theme changes.
- UI actions call back into the layout, grid, editor, object-authoring session, and tooling modules, keeping UI-specific logic out of the core systems while `Global_OnLayoutSaved/Loaded` and `Global_OnSceneLoaded` keep the dirty flag and source metadata in sync. In object workspace, primitive creation mirrors live layout bodies into the attached `ObjectAuthoringSession`, selects the created asset body, and clears scene-style resize handles so the evaluated topology overlay can emit vertex/edge hitboxes immediately. The link-handles button mirrors the `L` keyboard shortcut, the pin button mirrors `P`, `Fit B->Obj` routes through `Layout_FitSceneBounds3DToObject(...)`, scene import routes through `Tools/scene_import.c`, scene export routes through `Tools/scene_export.c`, and the overlay/marquee visuals ensure the user can see multi-selection and drag state even when the mouse is off-screen.
