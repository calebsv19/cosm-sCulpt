#include "Menu/line_drawing_host_menu_internal.h"

#include "Core/global_state.h"
#include "UI/ui_panel.h"
#include "UI/ui_panel_internal.h"

#include <SDL2/SDL.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static bool line_drawing_host_menu_path_exists(const char* path) {
    struct stat st = {0};
    if (!path || !path[0]) return false;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static void line_drawing_host_menu_clear_hover(LineDrawingHostMenuState* state);

bool line_drawing_host_menu_section_supports_filter(LineDrawingHostMenuSection section) {
    return section == LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS ||
           section == LINE_DRAWING_HOST_MENU_SECTION_SCENES;
}

static void line_drawing_host_menu_set_status(LineDrawingHostMenuState* state,
                                              const char* text,
                                              bool is_error) {
    if (!state) return;
    if (!text) text = "";
    snprintf(state->status_text, sizeof(state->status_text), "%s", text);
    state->status_is_error = is_error;
}

int line_drawing_host_menu_content_row_height(LineDrawingHostMenuSection section) {
    return (section == LINE_DRAWING_HOST_MENU_SECTION_QUICK_ACTIONS)
               ? LINE_DRAWING_HOST_MENU_QUICK_ROW_HEIGHT
               : LINE_DRAWING_HOST_MENU_CATALOG_ROW_HEIGHT;
}

static void line_drawing_host_menu_stop_filter_editing(LineDrawingHostMenuState* state) {
    if (!state) return;
    state->filter_editing = false;
    if (SDL_IsTextInputActive()) {
        SDL_StopTextInput();
    }
}

static void line_drawing_host_menu_stop_scrollbar_drag(LineDrawingHostMenuState* state) {
    if (!state) return;
    state->scrollbar_dragging = false;
    state->scrollbar_drag_start_y = 0;
    state->scrollbar_drag_start_offset_px = 0.0f;
}

static void line_drawing_host_menu_begin_filter_editing(LineDrawingHostMenuState* state) {
    if (!state) return;
    if (!line_drawing_host_menu_section_supports_filter(state->selected_section)) return;
    state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_FILTER;
    state->filter_editing = true;
    if (!SDL_IsTextInputActive()) {
        SDL_StartTextInput();
    }
}

static void line_drawing_host_menu_apply_filter(LineDrawingHostMenuState* state) {
    int i = 0;
    int current_layout_actual = -1;
    int current_scene_actual = -1;
    int preferred_layout_visible = -1;
    int preferred_scene_visible = -1;
    int active_layout_visible = -1;
    int active_scene_visible = -1;

    if (!state) return;
    if (state->selected_layout_index >= 0 && state->selected_layout_index < state->filtered_layout_count) {
        current_layout_actual = state->filtered_layout_indices[state->selected_layout_index];
    }
    if (state->selected_scene_index >= 0 && state->selected_scene_index < state->filtered_scene_count) {
        current_scene_actual = state->filtered_scene_indices[state->selected_scene_index];
    }
    state->filtered_layout_count = 0;
    state->filtered_scene_count = 0;

    for (i = 0; i < state->catalog.layout_count && i < MAX_CONFIG_FILES; ++i) {
        const LineDrawingSceneCatalogEntry* entry = LineDrawingSceneCatalog_GetLayout(&state->catalog, i);
        if (LineDrawingSceneCatalog_EntryMatchesQuery(entry, state->filter_query)) {
            if (i == state->catalog.active_layout_index) {
                active_layout_visible = state->filtered_layout_count;
            }
            if (i == current_layout_actual) {
                preferred_layout_visible = state->filtered_layout_count;
            }
            state->filtered_layout_indices[state->filtered_layout_count++] = i;
        }
    }

    for (i = 0; i < state->catalog.scene_count && i < MAX_CONFIG_FILES; ++i) {
        const LineDrawingSceneCatalogEntry* entry = LineDrawingSceneCatalog_GetScene(&state->catalog, i);
        if (LineDrawingSceneCatalog_EntryMatchesQuery(entry, state->filter_query)) {
            if (i == state->catalog.active_scene_index) {
                active_scene_visible = state->filtered_scene_count;
            }
            if (i == current_scene_actual) {
                preferred_scene_visible = state->filtered_scene_count;
            }
            state->filtered_scene_indices[state->filtered_scene_count++] = i;
        }
    }

    if (state->filtered_layout_count <= 0) {
        state->selected_layout_index = -1;
        state->layout_scroll_px = 0.0f;
    } else if (state->selected_layout_index < 0 ||
               state->selected_layout_index >= state->filtered_layout_count) {
        state->selected_layout_index = preferred_layout_visible >= 0
                                           ? preferred_layout_visible
                                           : (active_layout_visible >= 0 ? active_layout_visible : 0);
    } else if (preferred_layout_visible >= 0) {
        state->selected_layout_index = preferred_layout_visible;
    }

    if (state->filtered_scene_count <= 0) {
        state->selected_scene_index = -1;
        state->scene_scroll_px = 0.0f;
    } else if (state->selected_scene_index < 0 ||
               state->selected_scene_index >= state->filtered_scene_count) {
        state->selected_scene_index = preferred_scene_visible >= 0
                                          ? preferred_scene_visible
                                          : (active_scene_visible >= 0 ? active_scene_visible : 0);
    } else if (preferred_scene_visible >= 0) {
        state->selected_scene_index = preferred_scene_visible;
    }
}

const LineDrawingSceneCatalogEntry* line_drawing_host_menu_selected_catalog_entry(
    const LineDrawingHostMenuState* state,
    LineDrawingHostMenuSection section,
    bool* out_is_active) {
    int visible_index = -1;
    int actual_index = -1;
    if (out_is_active) *out_is_active = false;
    if (!state) return NULL;

    if (section == LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS) {
        visible_index = state->selected_layout_index;
        if (visible_index < 0 || visible_index >= state->filtered_layout_count) return NULL;
        actual_index = state->filtered_layout_indices[visible_index];
        if (out_is_active) *out_is_active = (actual_index == state->catalog.active_layout_index);
        return LineDrawingSceneCatalog_GetLayout(&state->catalog, actual_index);
    }
    if (section == LINE_DRAWING_HOST_MENU_SECTION_SCENES) {
        visible_index = state->selected_scene_index;
        if (visible_index < 0 || visible_index >= state->filtered_scene_count) return NULL;
        actual_index = state->filtered_scene_indices[visible_index];
        if (out_is_active) *out_is_active = (actual_index == state->catalog.active_scene_index);
        return LineDrawingSceneCatalog_GetScene(&state->catalog, actual_index);
    }
    return NULL;
}

const LineDrawingRootBrowserEntry* line_drawing_host_menu_selected_browser_entry(
    const LineDrawingHostMenuState* state) {
    if (!state) return NULL;
    return LineDrawingRootBrowser_GetEntry(&state->browser, state->selected_browser_index);
}

const LineDrawingRecentMenuEntry* line_drawing_host_menu_selected_recent_entry(
    const LineDrawingHostMenuState* state) {
    if (!state) return NULL;
    return LineDrawingRecentMenuList_GetEntry(&state->recent_entries, state->selected_recent_index);
}

static LineDrawingCatalogPreviewSourceKind line_drawing_host_menu_preview_kind_for_section(
    LineDrawingHostMenuSection section) {
    return (section == LINE_DRAWING_HOST_MENU_SECTION_SCENES)
               ? LINE_DRAWING_CATALOG_PREVIEW_SOURCE_SCENE
               : LINE_DRAWING_CATALOG_PREVIEW_SOURCE_LAYOUT;
}

const LineDrawingCatalogPreviewData* line_drawing_host_menu_preview_for_entry(
    LineDrawingHostMenuState* state,
    LineDrawingHostMenuSection section,
    const LineDrawingSceneCatalogEntry* entry) {
    if (!state || !entry || !entry->path[0]) return NULL;
    if (section != LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS &&
        section != LINE_DRAWING_HOST_MENU_SECTION_SCENES) {
        return NULL;
    }
    return LineDrawingCatalogPreviewCache_Get(&state->preview_cache,
                                              line_drawing_host_menu_preview_kind_for_section(section),
                                              entry->path);
}

const LineDrawingCatalogPreviewData* line_drawing_host_menu_preview_for_recent_entry(
    LineDrawingHostMenuState* state,
    const LineDrawingRecentMenuEntry* entry) {
    if (!state || !entry || !entry->path[0]) return NULL;
    switch (entry->kind) {
        case LINE_DRAWING_RECENT_MENU_ENTRY_LAYOUT:
            return LineDrawingCatalogPreviewCache_Get(&state->preview_cache,
                                                      LINE_DRAWING_CATALOG_PREVIEW_SOURCE_LAYOUT,
                                                      entry->path);
        case LINE_DRAWING_RECENT_MENU_ENTRY_SCENE:
            return LineDrawingCatalogPreviewCache_Get(&state->preview_cache,
                                                      LINE_DRAWING_CATALOG_PREVIEW_SOURCE_SCENE,
                                                      entry->path);
        default:
            return NULL;
    }
}

const LineDrawingCatalogPreviewData* line_drawing_host_menu_preview_for_browser_entry(
    LineDrawingHostMenuState* state,
    const LineDrawingRootBrowserEntry* entry) {
    if (!state || !entry || !entry->preview_path[0]) return NULL;
    return LineDrawingCatalogPreviewCache_Get(&state->preview_cache,
                                              entry->preview_kind,
                                              entry->preview_path);
}

static void line_drawing_host_menu_refresh_catalog(LineDrawingHostMenuState* state) {
    if (!state) return;
    LineDrawingRecentMenuList_Refresh(&state->recent_entries,
                                      Global_GetRecentContexts(),
                                      Global_GetLastLayoutPath(),
                                      Global_GetLastSceneAuthoringPath(),
                                      Global_GetInputRoot(),
                                      Global_GetOutputRoot());
    LineDrawingSceneCatalog_Refresh(&state->catalog,
                                    Global_GetInputRoot(),
                                    Global_GetCurrentConfigPath(),
                                    Global_GetCurrentSceneAuthoringPath());
    LineDrawingRootBrowser_Refresh(&state->browser,
                                   state->browser.current_path,
                                   Global_GetInputRoot(),
                                   Global_GetOutputRoot());
    {
        LineDrawingHostMenuModel model;
        LineDrawingHostMenu_BuildModel(&model);
        if (state->selected_index < 0 ||
            state->selected_index >= LINE_DRAWING_HOST_MENU_ITEM_COUNT ||
            !model.item_enabled[state->selected_index]) {
            state->selected_index = LineDrawingHostMenu_FirstSelectableIndex(&model);
        }
    }

    if (state->selected_section < 0 || state->selected_section >= LINE_DRAWING_HOST_MENU_SECTION_COUNT) {
        state->selected_section = LINE_DRAWING_HOST_MENU_SECTION_QUICK_ACTIONS;
    }
    if (!line_drawing_host_menu_section_supports_filter(state->selected_section)) {
        line_drawing_host_menu_stop_filter_editing(state);
        if (state->focus_region == LINE_DRAWING_HOST_MENU_FOCUS_FILTER) {
            state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
        }
    }
    line_drawing_host_menu_apply_filter(state);
    if (state->recent_entries.entry_count <= 0) {
        state->selected_recent_index = -1;
        state->recent_scroll_px = 0.0f;
    } else if (state->selected_recent_index < 0 ||
               state->selected_recent_index >= state->recent_entries.entry_count) {
        state->selected_recent_index = 0;
    }
    if (state->browser.entry_count <= 0) {
        state->selected_browser_index = -1;
        state->browser_scroll_px = 0.0f;
    } else if (state->selected_browser_index < 0 ||
               state->selected_browser_index >= state->browser.entry_count) {
        state->selected_browser_index = 0;
    }
}

int line_drawing_host_menu_content_count(const LineDrawingHostMenuState* state,
                                         LineDrawingHostMenuSection section) {
    if (!state) return 0;
    switch (section) {
        case LINE_DRAWING_HOST_MENU_SECTION_QUICK_ACTIONS:
            return LINE_DRAWING_HOST_MENU_ITEM_COUNT;
        case LINE_DRAWING_HOST_MENU_SECTION_RECENTS:
            return state->recent_entries.entry_count;
        case LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS:
            return state->filtered_layout_count;
        case LINE_DRAWING_HOST_MENU_SECTION_SCENES:
            return state->filtered_scene_count;
        case LINE_DRAWING_HOST_MENU_SECTION_BROWSE:
            return state->browser.entry_count;
        default:
            return 0;
    }
}

static int line_drawing_host_menu_move_content_selection(const LineDrawingHostMenuState* state,
                                                         const LineDrawingHostMenuModel* model,
                                                         LineDrawingHostMenuSection section,
                                                         int current_index,
                                                         int direction) {
    int count = line_drawing_host_menu_content_count(state, section);
    int index = current_index;
    int steps = 0;
    if (!state) return current_index;
    if (section == LINE_DRAWING_HOST_MENU_SECTION_QUICK_ACTIONS) {
        return LineDrawingHostMenu_MoveSelection(model, current_index, direction);
    }
    if (direction == 0 || count <= 0) return current_index;
    if (current_index < 0 || current_index >= count) return 0;
    while (steps < count) {
        index += direction > 0 ? 1 : -1;
        if (index < 0) index = count - 1;
        if (index >= count) index = 0;
        return index;
    }
    return current_index;
}


static bool line_drawing_host_menu_handle_root_folder_shortcut(LineDrawingHostMenuState* state,
                                                               bool output_root) {
    bool changed = false;
    if (!state) return false;

    changed = output_root ? UIPanel_OpenOutputRootFolderDialog()
                          : UIPanel_OpenInputRootFolderDialog();
    if (changed) {
        snprintf(state->browser.current_path,
                 sizeof(state->browser.current_path),
                 "%s",
                 Global_GetInputRoot() ? Global_GetInputRoot() : "");
        line_drawing_host_menu_refresh_catalog(state);
        line_drawing_host_menu_set_status(state,
                                          output_root
                                              ? "Output root updated from native folder picker."
                                              : "Input root updated from native folder picker.",
                                          false);
    } else {
        line_drawing_host_menu_set_status(state,
                                          output_root
                                              ? "Output root unchanged."
                                              : "Input root unchanged.",
                                          false);
    }
    return true;
}

static bool line_drawing_host_menu_activate_browse_action(LineDrawingHostMenuState* state,
                                                          int action_index) {
    if (!state) return false;
    switch ((LineDrawingHostMenuBrowseAction)action_index) {
        case LINE_DRAWING_HOST_MENU_BROWSE_ACTION_PICK_INPUT:
            return line_drawing_host_menu_handle_root_folder_shortcut(state, false);
        case LINE_DRAWING_HOST_MENU_BROWSE_ACTION_PICK_OUTPUT:
            return line_drawing_host_menu_handle_root_folder_shortcut(state, true);
        default:
            return false;
    }
}

static void line_drawing_host_menu_enter_catalog_for_current_root(
    LineDrawingHostMenuState* state,
    LineDrawingCatalogPreviewSourceKind preferred_kind) {
    if (!state) return;
    if (preferred_kind == LINE_DRAWING_CATALOG_PREVIEW_SOURCE_SCENE &&
        state->catalog.scene_count > 0) {
        state->selected_section = LINE_DRAWING_HOST_MENU_SECTION_SCENES;
        state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
        if (state->selected_scene_index < 0) {
            state->selected_scene_index = 0;
        }
        return;
    }
    if (preferred_kind == LINE_DRAWING_CATALOG_PREVIEW_SOURCE_LAYOUT &&
        state->catalog.layout_count > 0) {
        state->selected_section = LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS;
        state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
        if (state->selected_layout_index < 0) {
            state->selected_layout_index = 0;
        }
        return;
    }
    if (state->catalog.scene_count > 0) {
        state->selected_section = LINE_DRAWING_HOST_MENU_SECTION_SCENES;
        state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
        if (state->selected_scene_index < 0) {
            state->selected_scene_index = 0;
        }
        return;
    }
    if (state->catalog.layout_count > 0) {
        state->selected_section = LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS;
        state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
        if (state->selected_layout_index < 0) {
            state->selected_layout_index = 0;
        }
        return;
    }
    if (state->browser.entry_count > 0 && state->selected_browser_index < 0) {
        state->selected_browser_index = 0;
    }
}

static bool line_drawing_host_menu_activate_browser_entry(LineDrawingHostMenuState* state) {
    const LineDrawingRootBrowserEntry* entry = line_drawing_host_menu_selected_browser_entry(state);
    LineDrawingCatalogPreviewSourceKind preferred_kind =
        LINE_DRAWING_CATALOG_PREVIEW_SOURCE_LAYOUT;
    if (!state || !entry || !entry->enabled) return false;
    preferred_kind = entry->preview_kind;

    switch (entry->kind) {
        case LINE_DRAWING_ROOT_BROWSER_ENTRY_NEARBY_INPUT_ROOT:
            if (Global_SetInputRoot(entry->path, true)) {
                snprintf(state->browser.current_path, sizeof(state->browser.current_path), "%s", entry->path);
                line_drawing_host_menu_refresh_catalog(state);
                line_drawing_host_menu_enter_catalog_for_current_root(state, preferred_kind);
                line_drawing_host_menu_set_status(state,
                                                  "Input root switched and opened the catalog for the nearby directory.",
                                                  false);
            } else {
                line_drawing_host_menu_set_status(state, "Failed to update input root.", true);
            }
            return true;
        default:
            return false;
    }
}

static bool line_drawing_host_menu_activate_recent_entry(LineDrawingHostMenuState* state,
                                                         LineDrawingHostMenuCommand* out_command) {
    const LineDrawingRecentMenuEntry* entry = line_drawing_host_menu_selected_recent_entry(state);
    if (!state || !entry || !out_command) return false;

    switch (entry->kind) {
        case LINE_DRAWING_RECENT_MENU_ENTRY_LAYOUT:
            if (UIPanel_LoadLayoutFromPath(entry->path)) {
                out_command->type = LINE_DRAWING_HOST_MENU_COMMAND_OPEN_EDITOR;
                line_drawing_host_menu_set_status(state, "Opened recent layout.", false);
            } else {
                line_drawing_host_menu_set_status(state, "Failed to open recent layout.", true);
            }
            return true;
        case LINE_DRAWING_RECENT_MENU_ENTRY_SCENE:
            if (UIPanel_LoadSceneFromPath(entry->path)) {
                out_command->type = LINE_DRAWING_HOST_MENU_COMMAND_OPEN_EDITOR;
                line_drawing_host_menu_set_status(state, "Opened recent scene.", false);
            } else {
                line_drawing_host_menu_set_status(state, "Failed to open recent scene.", true);
            }
            return true;
        case LINE_DRAWING_RECENT_MENU_ENTRY_INPUT_ROOT:
            if (Global_SetInputRoot(entry->path, true)) {
                snprintf(state->browser.current_path, sizeof(state->browser.current_path), "%s", entry->path);
                line_drawing_host_menu_refresh_catalog(state);
                line_drawing_host_menu_set_status(state, "Switched to recent input root.", false);
            } else {
                line_drawing_host_menu_set_status(state, "Failed to switch input root.", true);
            }
            return true;
        case LINE_DRAWING_RECENT_MENU_ENTRY_OUTPUT_ROOT:
            if (Global_SetOutputRoot(entry->path, true)) {
                snprintf(state->browser.current_path, sizeof(state->browser.current_path), "%s", entry->path);
                line_drawing_host_menu_refresh_catalog(state);
                line_drawing_host_menu_set_status(state, "Switched to recent output root.", false);
            } else {
                line_drawing_host_menu_set_status(state, "Failed to switch output root.", true);
            }
            return true;
        default:
            return false;
    }
}

static bool line_drawing_host_menu_activate(LineDrawingHostMenuState* state,
                                            const LineDrawingHostMenuModel* model,
                                            LineDrawingHostMenuCommand* out_command) {
    const char* current_layout = Global_GetLastLayoutPath();
    const char* current_scene = Global_GetLastSceneAuthoringPath();
    const LineDrawingSceneCatalogEntry* entry = NULL;

    if (!state || !model || !out_command) return false;
    memset(out_command, 0, sizeof(*out_command));

    switch (state->selected_section) {
        case LINE_DRAWING_HOST_MENU_SECTION_QUICK_ACTIONS:
            if (state->selected_index < 0 || state->selected_index >= LINE_DRAWING_HOST_MENU_ITEM_COUNT) {
                return false;
            }
            if (!model->item_enabled[state->selected_index]) {
                line_drawing_host_menu_set_status(state,
                                                  "Selection is unavailable for the current session.",
                                                  true);
                return true;
            }
            switch ((LineDrawingHostMenuItemId)state->selected_index) {
                case LINE_DRAWING_HOST_MENU_ITEM_RESUME_EDITOR:
                    line_drawing_host_menu_stop_filter_editing(state);
                    out_command->type = LINE_DRAWING_HOST_MENU_COMMAND_OPEN_EDITOR;
                    line_drawing_host_menu_set_status(state, "Opening editor.", false);
                    return true;
                case LINE_DRAWING_HOST_MENU_ITEM_LOAD_LAST_LAYOUT:
                    if (UIPanel_LoadLayoutFromPath(current_layout)) {
                        line_drawing_host_menu_stop_filter_editing(state);
                        out_command->type = LINE_DRAWING_HOST_MENU_COMMAND_OPEN_EDITOR;
                        line_drawing_host_menu_set_status(state, "Reopened last layout.", false);
                    } else {
                        line_drawing_host_menu_set_status(state, "Failed to reopen last layout.", true);
                    }
                    return true;
                case LINE_DRAWING_HOST_MENU_ITEM_LOAD_LAST_SCENE:
                    if (UIPanel_LoadSceneFromPath(current_scene)) {
                        line_drawing_host_menu_stop_filter_editing(state);
                        out_command->type = LINE_DRAWING_HOST_MENU_COMMAND_OPEN_EDITOR;
                        line_drawing_host_menu_set_status(state, "Reopened last scene.", false);
                    } else {
                        line_drawing_host_menu_set_status(state, "Failed to reopen last scene.", true);
                    }
                    return true;
                case LINE_DRAWING_HOST_MENU_ITEM_QUIT:
                    out_command->type = LINE_DRAWING_HOST_MENU_COMMAND_QUIT;
                    return true;
                default:
                    break;
            }
            return false;
        case LINE_DRAWING_HOST_MENU_SECTION_RECENTS:
            if (!line_drawing_host_menu_activate_recent_entry(state, out_command)) {
                line_drawing_host_menu_set_status(state, "No recent context is available to activate.", true);
            }
            return true;
        case LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS:
            entry = line_drawing_host_menu_selected_catalog_entry(state,
                                                                  LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS,
                                                                  NULL);
            if (!entry) {
                line_drawing_host_menu_set_status(state, "No layout entry is available to open.", true);
                return true;
            }
            if (UIPanel_LoadLayoutFromPath(entry->path)) {
                line_drawing_host_menu_stop_filter_editing(state);
                out_command->type = LINE_DRAWING_HOST_MENU_COMMAND_OPEN_EDITOR;
                line_drawing_host_menu_set_status(state, "Opened layout from catalog.", false);
            } else {
                line_drawing_host_menu_set_status(state, "Failed to open selected layout.", true);
            }
            return true;
        case LINE_DRAWING_HOST_MENU_SECTION_SCENES:
            entry = line_drawing_host_menu_selected_catalog_entry(state,
                                                                  LINE_DRAWING_HOST_MENU_SECTION_SCENES,
                                                                  NULL);
            if (!entry) {
                line_drawing_host_menu_set_status(state, "No scene entry is available to open.", true);
                return true;
            }
            if (UIPanel_LoadSceneFromPath(entry->path)) {
                line_drawing_host_menu_stop_filter_editing(state);
                out_command->type = LINE_DRAWING_HOST_MENU_COMMAND_OPEN_EDITOR;
                line_drawing_host_menu_set_status(state, "Opened scene from catalog.", false);
            } else {
                line_drawing_host_menu_set_status(state, "Failed to open selected scene.", true);
            }
            return true;
        case LINE_DRAWING_HOST_MENU_SECTION_BROWSE:
            if (!line_drawing_host_menu_activate_browser_entry(state)) {
                line_drawing_host_menu_set_status(state, "No browser entry is available to activate.", true);
            }
            return true;
        default:
            return false;
    }
}

void LineDrawingHostMenu_Init(LineDrawingHostMenuState* state) {
    if (!state) return;
    memset(state, 0, sizeof(*state));
    state->selected_index = LINE_DRAWING_HOST_MENU_ITEM_RESUME_EDITOR;
    state->selected_section = LINE_DRAWING_HOST_MENU_SECTION_QUICK_ACTIONS;
    state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
    state->hovered_section_index = -1;
    state->hovered_content_index = -1;
    state->hovered_browse_action_index = -1;
    state->scrollbar_dragging = false;
    state->selected_recent_index = -1;
    state->selected_layout_index = -1;
    state->selected_scene_index = -1;
    state->selected_browser_index = 0;
    LineDrawingRecentMenuList_Init(&state->recent_entries);
    LineDrawingSceneCatalog_Init(&state->catalog);
    LineDrawingRootBrowser_Init(&state->browser);
    LineDrawingCatalogPreviewCache_Init(&state->preview_cache);
    snprintf(state->status_text,
             sizeof(state->status_text),
             "Phase 2 menu work: recents, catalog, and root-context controls are active.");
}

void LineDrawingHostMenu_BuildModel(LineDrawingHostMenuModel* out_model) {
    const char* current_layout = Global_GetLastLayoutPath();
    const char* current_scene = Global_GetLastSceneAuthoringPath();
    if (!out_model) return;
    memset(out_model, 0, sizeof(*out_model));
    out_model->item_enabled[LINE_DRAWING_HOST_MENU_ITEM_RESUME_EDITOR] = true;
    out_model->item_enabled[LINE_DRAWING_HOST_MENU_ITEM_LOAD_LAST_LAYOUT] =
        line_drawing_host_menu_path_exists(current_layout);
    out_model->item_enabled[LINE_DRAWING_HOST_MENU_ITEM_LOAD_LAST_SCENE] =
        line_drawing_host_menu_path_exists(current_scene);
    out_model->item_enabled[LINE_DRAWING_HOST_MENU_ITEM_QUIT] = true;
}

int LineDrawingHostMenu_FirstSelectableIndex(const LineDrawingHostMenuModel* model) {
    int i = 0;
    if (!model) return -1;
    for (i = 0; i < LINE_DRAWING_HOST_MENU_ITEM_COUNT; ++i) {
        if (model->item_enabled[i]) return i;
    }
    return -1;
}

int LineDrawingHostMenu_MoveSelection(const LineDrawingHostMenuModel* model,
                                      int current_index,
                                      int direction) {
    int index = current_index;
    int steps = 0;
    if (!model) return current_index;
    if (direction == 0) return current_index;
    if (current_index < 0 || current_index >= LINE_DRAWING_HOST_MENU_ITEM_COUNT) {
        return LineDrawingHostMenu_FirstSelectableIndex(model);
    }
    while (steps < LINE_DRAWING_HOST_MENU_ITEM_COUNT) {
        index += direction > 0 ? 1 : -1;
        if (index < 0) index = LINE_DRAWING_HOST_MENU_ITEM_COUNT - 1;
        if (index >= LINE_DRAWING_HOST_MENU_ITEM_COUNT) index = 0;
        if (model->item_enabled[index]) return index;
        steps += 1;
    }
    return current_index;
}

bool LineDrawingHostMenu_HandleEvent(LineDrawingHostMenuState* state,
                                     AppContext* ctx,
                                     const SDL_Event* event,
                                     LineDrawingHostMenuCommand* out_command) {
    LineDrawingHostMenuModel model;
    LineDrawingHostMenuLayout layout;
    int hovered_index = -1;
    int* selected_ptr = NULL;
    int nav_index = -1;
    int browse_action_index = -1;
    bool filter_hit = false;
    int x = 0;
    int y = 0;
    (void)ctx;
    if (!state || !event || !out_command) return false;
    memset(out_command, 0, sizeof(*out_command));

    line_drawing_host_menu_refresh_catalog(state);
    LineDrawingHostMenu_BuildModel(&model);
    line_drawing_host_menu_layout(&layout,
                                  Global_GetScreenWidth(),
                                  Global_GetScreenHeight());
    line_drawing_host_menu_clear_hover(state);

    switch (event->type) {
        case SDL_KEYDOWN:
            line_drawing_host_menu_stop_scrollbar_drag(state);
            if ((SDL_GetModState() & (KMOD_CTRL | KMOD_GUI)) != 0) {
                if ((SDL_GetModState() & KMOD_SHIFT) != 0 && event->key.keysym.sym == SDLK_b) {
                    line_drawing_host_menu_stop_filter_editing(state);
                    line_drawing_host_menu_stop_scrollbar_drag(state);
                    return line_drawing_host_menu_handle_root_folder_shortcut(state, true);
                }
                if (event->key.keysym.sym == SDLK_b) {
                    line_drawing_host_menu_stop_filter_editing(state);
                    line_drawing_host_menu_stop_scrollbar_drag(state);
                    return line_drawing_host_menu_handle_root_folder_shortcut(state, false);
                }
            }
            switch (event->key.keysym.sym) {
                case SDLK_UP:
                    if (state->focus_region == LINE_DRAWING_HOST_MENU_FOCUS_NAV) {
                        line_drawing_host_menu_stop_filter_editing(state);
                        state->selected_section =
                            (LineDrawingHostMenuSection)((state->selected_section +
                                                           LINE_DRAWING_HOST_MENU_SECTION_COUNT - 1) %
                                                          LINE_DRAWING_HOST_MENU_SECTION_COUNT);
                    } else {
                        selected_ptr =
                            line_drawing_host_menu_selected_content_index_ptr(state,
                                                                              state->selected_section);
                        if (selected_ptr) {
                            *selected_ptr = line_drawing_host_menu_move_content_selection(state,
                                                                                          &model,
                                                                                          state->selected_section,
                                                                                          *selected_ptr,
                                                                                          -1);
                            line_drawing_host_menu_ensure_selected_visible(state,
                                                                           &layout,
                                                                           state->selected_section);
                        }
                    }
                    return true;
                case SDLK_DOWN:
                    if (state->focus_region == LINE_DRAWING_HOST_MENU_FOCUS_NAV) {
                        line_drawing_host_menu_stop_filter_editing(state);
                        state->selected_section =
                            (LineDrawingHostMenuSection)((state->selected_section + 1) %
                                                          LINE_DRAWING_HOST_MENU_SECTION_COUNT);
                    } else {
                        selected_ptr =
                            line_drawing_host_menu_selected_content_index_ptr(state,
                                                                              state->selected_section);
                        if (selected_ptr) {
                            *selected_ptr = line_drawing_host_menu_move_content_selection(state,
                                                                                          &model,
                                                                                          state->selected_section,
                                                                                          *selected_ptr,
                                                                                          1);
                            line_drawing_host_menu_ensure_selected_visible(state,
                                                                           &layout,
                                                                           state->selected_section);
                        }
                    }
                    return true;
                case SDLK_LEFT:
                    line_drawing_host_menu_stop_filter_editing(state);
                    state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_NAV;
                    return true;
                case SDLK_RIGHT:
                    if (line_drawing_host_menu_section_supports_filter(state->selected_section)) {
                        state->focus_region = (state->focus_region == LINE_DRAWING_HOST_MENU_FOCUS_NAV)
                                                  ? LINE_DRAWING_HOST_MENU_FOCUS_FILTER
                                                  : LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
                    } else {
                        line_drawing_host_menu_stop_filter_editing(state);
                        state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
                    }
                    return true;
                case SDLK_TAB:
                    if (line_drawing_host_menu_section_supports_filter(state->selected_section)) {
                        if (state->focus_region == LINE_DRAWING_HOST_MENU_FOCUS_NAV) {
                            line_drawing_host_menu_begin_filter_editing(state);
                        } else if (state->focus_region == LINE_DRAWING_HOST_MENU_FOCUS_FILTER) {
                            line_drawing_host_menu_stop_filter_editing(state);
                            state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
                        } else {
                            state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_NAV;
                        }
                    } else {
                        line_drawing_host_menu_stop_filter_editing(state);
                        state->focus_region = (state->focus_region == LINE_DRAWING_HOST_MENU_FOCUS_NAV)
                                                  ? LINE_DRAWING_HOST_MENU_FOCUS_CONTENT
                                                  : LINE_DRAWING_HOST_MENU_FOCUS_NAV;
                    }
                    return true;
                case SDLK_SLASH:
                    if (line_drawing_host_menu_section_supports_filter(state->selected_section)) {
                        line_drawing_host_menu_begin_filter_editing(state);
                        return true;
                    }
                    break;
                case SDLK_BACKSPACE:
                    if (state->focus_region == LINE_DRAWING_HOST_MENU_FOCUS_FILTER) {
                        size_t len = strlen(state->filter_query);
                        if (len > 0u) {
                            state->filter_query[len - 1u] = '\0';
                            line_drawing_host_menu_apply_filter(state);
                            line_drawing_host_menu_set_status(state, "Catalog filter updated.", false);
                        }
                        return true;
                    }
                    break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                case SDLK_SPACE:
                    if (state->focus_region == LINE_DRAWING_HOST_MENU_FOCUS_NAV) {
                        if (line_drawing_host_menu_section_supports_filter(state->selected_section)) {
                            line_drawing_host_menu_begin_filter_editing(state);
                        } else {
                            state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
                        }
                        return true;
                    }
                    if (state->focus_region == LINE_DRAWING_HOST_MENU_FOCUS_FILTER) {
                        line_drawing_host_menu_stop_filter_editing(state);
                        state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
                        return true;
                    }
                    return line_drawing_host_menu_activate(state, &model, out_command);
                case SDLK_ESCAPE:
                    if (state->focus_region == LINE_DRAWING_HOST_MENU_FOCUS_FILTER) {
                        if (state->filter_query[0]) {
                            state->filter_query[0] = '\0';
                            line_drawing_host_menu_apply_filter(state);
                            line_drawing_host_menu_set_status(state, "Catalog filter cleared.", false);
                        } else {
                            line_drawing_host_menu_stop_filter_editing(state);
                            state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
                        }
                        return true;
                    }
                    out_command->type = LINE_DRAWING_HOST_MENU_COMMAND_QUIT;
                    return true;
                default:
                    break;
            }
            break;
        case SDL_TEXTINPUT:
            if (state->focus_region == LINE_DRAWING_HOST_MENU_FOCUS_FILTER &&
                line_drawing_host_menu_section_supports_filter(state->selected_section) &&
                event->text.text[0]) {
                size_t len = strlen(state->filter_query);
                size_t add_len = strlen(event->text.text);
                if (len + add_len >= sizeof(state->filter_query)) {
                    add_len = sizeof(state->filter_query) - len - 1u;
                }
                if (add_len > 0u) {
                    memcpy(state->filter_query + len, event->text.text, add_len);
                    state->filter_query[len + add_len] = '\0';
                    line_drawing_host_menu_apply_filter(state);
                    line_drawing_host_menu_set_status(state, "Catalog filter updated.", false);
                }
                return true;
            }
            break;
        case SDL_MOUSEMOTION:
            if (state->scrollbar_dragging) {
                return line_drawing_host_menu_scrollbar_drag_to(state,
                                                                &layout,
                                                                state->selected_section,
                                                                event->motion.y);
            }
            nav_index = line_drawing_host_menu_hit_test_nav(&layout,
                                                            event->motion.x,
                                                            event->motion.y);
            if (nav_index >= 0) {
                state->hovered_section_index = nav_index;
                return true;
            }
            filter_hit = line_drawing_host_menu_hit_test_filter(state,
                                                                &layout,
                                                                event->motion.x,
                                                                event->motion.y);
            if (filter_hit) {
                state->hovered_filter = true;
                return true;
            }
            browse_action_index = line_drawing_host_menu_hit_test_browse_action(state,
                                                                                &layout,
                                                                                event->motion.x,
                                                                                event->motion.y);
            if (browse_action_index >= 0) {
                state->hovered_browse_action_index = browse_action_index;
                return true;
            }
            hovered_index = line_drawing_host_menu_hit_test_content(state,
                                                                    &layout,
                                                                    event->motion.x,
                                                                    event->motion.y);
            if (hovered_index >= 0) {
                state->hovered_content_index = hovered_index;
                return true;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (event->button.button != SDL_BUTTON_LEFT) break;
            nav_index = line_drawing_host_menu_hit_test_nav(&layout,
                                                            event->button.x,
                                                            event->button.y);
            if (nav_index >= 0) {
                line_drawing_host_menu_stop_scrollbar_drag(state);
                state->selected_section = (LineDrawingHostMenuSection)nav_index;
                state->hovered_section_index = nav_index;
                if (line_drawing_host_menu_section_supports_filter(state->selected_section)) {
                    line_drawing_host_menu_begin_filter_editing(state);
                } else {
                    line_drawing_host_menu_stop_filter_editing(state);
                    state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
                }
                return true;
            }
            filter_hit = line_drawing_host_menu_hit_test_filter(state,
                                                                &layout,
                                                                event->button.x,
                                                                event->button.y);
            if (filter_hit) {
                line_drawing_host_menu_stop_scrollbar_drag(state);
                state->hovered_filter = true;
                line_drawing_host_menu_begin_filter_editing(state);
                return true;
            }
            browse_action_index = line_drawing_host_menu_hit_test_browse_action(state,
                                                                                &layout,
                                                                                event->button.x,
                                                                                event->button.y);
            if (browse_action_index >= 0) {
                line_drawing_host_menu_stop_scrollbar_drag(state);
                line_drawing_host_menu_stop_filter_editing(state);
                state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
                state->hovered_browse_action_index = browse_action_index;
                return line_drawing_host_menu_activate_browse_action(state, browse_action_index);
            }
            if (line_drawing_host_menu_hit_test_scrollbar_track(state,
                                                                &layout,
                                                                event->button.x,
                                                                event->button.y)) {
                SDL_Rect thumb = line_drawing_host_menu_scrollbar_thumb_rect(state,
                                                                             &layout,
                                                                             state->selected_section);
                line_drawing_host_menu_stop_filter_editing(state);
                state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
                if (SDL_PointInRect(&(SDL_Point){event->button.x, event->button.y}, &thumb)) {
                    state->scrollbar_dragging = true;
                    state->scrollbar_drag_start_y = event->button.y;
                    {
                        float* scroll =
                            line_drawing_host_menu_scroll_ptr(state, state->selected_section);
                        state->scrollbar_drag_start_offset_px = scroll ? *scroll : 0.0f;
                    }
                } else {
                    (void)line_drawing_host_menu_jump_scrollbar_to(state,
                                                                   &layout,
                                                                   state->selected_section,
                                                                   event->button.y);
                }
                return true;
            }
            hovered_index = line_drawing_host_menu_hit_test_content(state,
                                                                    &layout,
                                                                    event->button.x,
                                                                    event->button.y);
            if (hovered_index >= 0) {
                selected_ptr =
                    line_drawing_host_menu_selected_content_index_ptr(state,
                                                                      state->selected_section);
                if (selected_ptr) {
                    *selected_ptr = hovered_index;
                }
                state->hovered_content_index = hovered_index;
                line_drawing_host_menu_stop_filter_editing(state);
                state->focus_region = LINE_DRAWING_HOST_MENU_FOCUS_CONTENT;
                return line_drawing_host_menu_activate(state, &model, out_command);
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (event->button.button == SDL_BUTTON_LEFT && state->scrollbar_dragging) {
                line_drawing_host_menu_stop_scrollbar_drag(state);
                return true;
            }
            break;
        case SDL_MOUSEWHEEL:
            SDL_GetMouseState(&x, &y);
            if (SDL_PointInRect(&(SDL_Point){x, y}, &layout.list_view_rect) ||
                SDL_PointInRect(&(SDL_Point){x, y}, &layout.list_scrollbar_rect)) {
                float* scroll = line_drawing_host_menu_scroll_ptr(state, state->selected_section);
                if (scroll) {
                    *scroll -= (float)event->wheel.y *
                               (float)(line_drawing_host_menu_content_row_height(state->selected_section) * 3);
                    line_drawing_host_menu_clamp_scroll(state, &layout, state->selected_section);
                    return true;
                }
            }
            break;
        default:
            break;
    }

    return false;
}

static void line_drawing_host_menu_clear_hover(LineDrawingHostMenuState* state) {
    if (!state) return;
    state->hovered_section_index = -1;
    state->hovered_content_index = -1;
    state->hovered_browse_action_index = -1;
    state->hovered_filter = false;
}
