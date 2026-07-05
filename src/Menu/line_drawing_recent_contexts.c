#include "Menu/line_drawing_recent_contexts.h"

#include "Core/line_drawing_file_catalog.h"

#include <stdio.h>
#include <string.h>

static const char* line_drawing_recent_contexts_basename(const char* path) {
    if (!path || !path[0]) return "";
    return LineDrawingFileCatalog_PathBasename(path);
}

static void line_drawing_recent_contexts_last_path_component(const char* path,
                                                             char* out,
                                                             size_t out_size) {
    const char* basename = line_drawing_recent_contexts_basename(path);
    if (!out || out_size == 0u) return;
    if (basename && basename[0]) {
        snprintf(out, out_size, "%s", basename);
        return;
    }
    snprintf(out, out_size, "%s", path ? path : "");
}

static void line_drawing_recent_contexts_add_entry(LineDrawingRecentMenuList* list,
                                                   LineDrawingRecentMenuEntryKind kind,
                                                   const char* label,
                                                   const char* description,
                                                   const char* path,
                                                   bool current) {
    LineDrawingRecentMenuEntry* entry = NULL;
    if (!list || list->entry_count < 0 || list->entry_count >= MAX_CONFIG_FILES) return;
    entry = &list->entries[list->entry_count++];
    memset(entry, 0, sizeof(*entry));
    entry->kind = kind;
    entry->current = current;
    snprintf(entry->label, sizeof(entry->label), "%s", label ? label : "");
    snprintf(entry->description, sizeof(entry->description), "%s", description ? description : "");
    snprintf(entry->path, sizeof(entry->path), "%s", path ? path : "");
    switch (kind) {
        case LINE_DRAWING_RECENT_MENU_ENTRY_LAYOUT:
            list->layout_count += 1;
            break;
        case LINE_DRAWING_RECENT_MENU_ENTRY_SCENE:
            list->scene_count += 1;
            break;
        case LINE_DRAWING_RECENT_MENU_ENTRY_INPUT_ROOT:
            list->input_root_count += 1;
            break;
        case LINE_DRAWING_RECENT_MENU_ENTRY_OUTPUT_ROOT:
            list->output_root_count += 1;
            break;
        default:
            break;
    }
}

static void line_drawing_recent_contexts_append_paths(LineDrawingRecentMenuList* list,
                                                      const LineDrawingRecentPathList* paths,
                                                      LineDrawingRecentMenuEntryKind kind,
                                                      const char* current_path) {
    int i = 0;
    if (!list || !paths) return;
    for (i = 0; i < paths->count && list->entry_count < MAX_CONFIG_FILES; ++i) {
        char label[128];
        char description[160];
        const bool current = current_path && current_path[0] &&
                             strcmp(paths->paths[i], current_path) == 0;

        switch (kind) {
            case LINE_DRAWING_RECENT_MENU_ENTRY_LAYOUT:
                snprintf(label, sizeof(label), "%s", line_drawing_recent_contexts_basename(paths->paths[i]));
                snprintf(description,
                         sizeof(description),
                         "%s",
                         current ? "Recent layout • current layout" : "Recent layout");
                break;
            case LINE_DRAWING_RECENT_MENU_ENTRY_SCENE:
                snprintf(label, sizeof(label), "%s", line_drawing_recent_contexts_basename(paths->paths[i]));
                snprintf(description,
                         sizeof(description),
                         "%s",
                         current ? "Recent scene • current scene" : "Recent scene");
                break;
            case LINE_DRAWING_RECENT_MENU_ENTRY_INPUT_ROOT:
                line_drawing_recent_contexts_last_path_component(paths->paths[i], label, sizeof(label));
                snprintf(description,
                         sizeof(description),
                         "%s",
                         current ? "Recent input root • active input root" : "Recent input root");
                break;
            case LINE_DRAWING_RECENT_MENU_ENTRY_OUTPUT_ROOT:
                line_drawing_recent_contexts_last_path_component(paths->paths[i], label, sizeof(label));
                snprintf(description,
                         sizeof(description),
                         "%s",
                         current ? "Recent output root • active output root" : "Recent output root");
                break;
            default:
                snprintf(label, sizeof(label), "%s", paths->paths[i]);
                snprintf(description, sizeof(description), "Recent context");
                break;
        }

        line_drawing_recent_contexts_add_entry(list,
                                               kind,
                                               label,
                                               description,
                                               paths->paths[i],
                                               current);
    }
}

void LineDrawingRecentMenuList_Init(LineDrawingRecentMenuList* list) {
    if (!list) return;
    memset(list, 0, sizeof(*list));
}

void LineDrawingRecentMenuList_Refresh(LineDrawingRecentMenuList* list,
                                       const LineDrawingRecentContexts* contexts,
                                       const char* current_layout_path,
                                       const char* current_scene_path,
                                       const char* current_input_root,
                                       const char* current_output_root) {
    if (!list) return;
    LineDrawingRecentMenuList_Init(list);
    if (!contexts) return;

    line_drawing_recent_contexts_append_paths(list,
                                              &contexts->layouts,
                                              LINE_DRAWING_RECENT_MENU_ENTRY_LAYOUT,
                                              current_layout_path);
    line_drawing_recent_contexts_append_paths(list,
                                              &contexts->scenes,
                                              LINE_DRAWING_RECENT_MENU_ENTRY_SCENE,
                                              current_scene_path);
    line_drawing_recent_contexts_append_paths(list,
                                              &contexts->input_roots,
                                              LINE_DRAWING_RECENT_MENU_ENTRY_INPUT_ROOT,
                                              current_input_root);
    line_drawing_recent_contexts_append_paths(list,
                                              &contexts->output_roots,
                                              LINE_DRAWING_RECENT_MENU_ENTRY_OUTPUT_ROOT,
                                              current_output_root);
}

const LineDrawingRecentMenuEntry* LineDrawingRecentMenuList_GetEntry(
    const LineDrawingRecentMenuList* list,
    int index) {
    if (!list || index < 0 || index >= list->entry_count) return NULL;
    return &list->entries[index];
}
