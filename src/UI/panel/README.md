# UI Panel

This subtree owns the editor-local pane shell and pane-scoped controls.

## Ownership

- Shell and state:
  - `ui_panel.c`
  - `ui_panel_shell.c`
  - `ui_panel_controls.c`
  - `input_ui_panel.c`
  - `render_ui_panel.c`
- File pane and persistent browser:
  - `ui_panel_file_*`
  - `ui_panel_load_menu.c`
  - `ui_panel_file_browser_*`
  - `ui_panel_file_status.c` owns File-pane-local display basename, browser
    mode labels, summary mode labels, and prefixed browser/action status-line
    formatting shared by the File summary and expanded browser.
- Scene pane:
  - `ui_panel_scene_*`
- Right pane summaries and layouts:
  - `ui_panel_view_*`
  - `ui_panel_create_*`
  - `ui_panel_object_*`
  - `ui_panel_edit_*`
  - object workspace summaries reuse the Core file-catalog basename helper for
    path display while keeping object/CAD labels and summary layout local.
- Shared panel presentation:
  - `ui_panel_summary_surface.c`
  - `ui_panel_visual_style.c`
  - `ui_panel_right_controls.c`

## Boundary

- Keep pane layout, button routing, summary surfaces, browser state, and
  panel-local click handling here.
- Keep File-pane wording and transient status presentation UI-panel-local; the
  import/export/tool operations still own their diagnostics, while runtime mesh
  session path/status storage is updated through Core `Global_*ObjectRuntimeMesh*`
  helpers.
- Common user-triggered File-pane failures should set a visible
  `UIPanel_SetFilePaneActionStatus(...)` message in addition to any SDL log or
  Core runtime mesh status. Async STL import failures should keep progress
  detail and runtime mesh status aligned without changing STL parser behavior.
- Keep File-browser open/visible state synchronized through
  `UIPanel_SetFileBrowserVisible(...)` and `UIPanel_CloseFileBrowser(...)`;
  behavior paths should use those helpers instead of writing `loadMenu.open`
  directly. Direct initialization of the state struct remains local to
  `ui_panel.c`.
- Keep File-browser persisted-session diagnostics read-only through
  `UIPanel_GetFileBrowserRestoreSummary(...)`; restore behavior remains local
  to `ui_panel_load_menu.c`, while tests can assert mode/root/active and
  remembered-entry decisions without changing File-pane behavior.
- Keep topbar behavior in `src/UI/topbar/`.
- Keep modal overlay rendering in `src/UI/overlay/`.
- Keep object/CAD document semantics in `src/ObjectAuthoring/`; panel code may
  expose controls and summaries, but it should not become the semantic model.
- Keep pure render and summary draw files read-only with respect to app state.
  `ui_panel_object_workspace_summary.c` is currently a mixed exception because
  it owns model-tree interaction handlers as well as summary rendering; new
  mutation paths there need focused tests and should move behind Core/UI
  coordinator helpers when they become reusable.
- Large files in this subtree are future R5/decomposition candidates. Do not
  add new feature lanes to `ui_panel_load_menu.c` or `ui_panel_file_ops.c`
  without first checking for a smaller sibling module.
