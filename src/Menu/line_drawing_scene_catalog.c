#include "Menu/line_drawing_scene_catalog.h"

#include <ctype.h>
#include <string.h>

static bool line_drawing_scene_catalog_contains_case_insensitive(const char* haystack,
                                                                 const char* needle) {
    size_t needle_len = 0u;
    size_t i = 0u;
    if (!needle || !needle[0]) return true;
    if (!haystack || !haystack[0]) return false;
    needle_len = strlen(needle);
    if (needle_len == 0u) return true;

    for (i = 0u; haystack[i] != '\0'; ++i) {
        size_t j = 0u;
        while (needle[j] != '\0' &&
               haystack[i + j] != '\0' &&
               tolower((unsigned char)haystack[i + j]) == tolower((unsigned char)needle[j])) {
            j += 1u;
        }
        if (needle[j] == '\0') return true;
    }
    return false;
}

static void line_drawing_scene_catalog_find_active_indices(LineDrawingSceneCatalog* catalog,
                                                           const char* current_layout_path,
                                                           const char* current_scene_path) {
    int i = 0;
    if (!catalog) return;
    catalog->active_layout_index = -1;
    catalog->active_scene_index = -1;

    if (current_layout_path && current_layout_path[0]) {
        for (i = 0; i < catalog->layout_count; ++i) {
            if (strcmp(catalog->layouts[i].path, current_layout_path) == 0) {
                catalog->active_layout_index = i;
                break;
            }
        }
    }

    if (current_scene_path && current_scene_path[0]) {
        for (i = 0; i < catalog->scene_count; ++i) {
            if (strcmp(catalog->scenes[i].path, current_scene_path) == 0) {
                catalog->active_scene_index = i;
                break;
            }
        }
    }
}

void LineDrawingSceneCatalog_Init(LineDrawingSceneCatalog* catalog) {
    if (!catalog) return;
    memset(catalog, 0, sizeof(*catalog));
    catalog->active_layout_index = -1;
    catalog->active_scene_index = -1;
}

void LineDrawingSceneCatalog_Refresh(LineDrawingSceneCatalog* catalog,
                                     const char* input_root,
                                     const char* current_layout_path,
                                     const char* current_scene_path) {
    if (!catalog) return;
    LineDrawingSceneCatalog_Init(catalog);
    if (input_root && input_root[0]) {
        snprintf(catalog->input_root, sizeof(catalog->input_root), "%s", input_root);
    }

    catalog->layout_count = LineDrawingFileCatalog_ScanLayoutEntries(catalog->layouts,
                                                                     MAX_CONFIG_FILES,
                                                                     input_root);
    catalog->scene_count = LineDrawingFileCatalog_ScanSceneEntries(catalog->scenes,
                                                                   MAX_CONFIG_FILES,
                                                                   input_root);
    line_drawing_scene_catalog_find_active_indices(catalog,
                                                   current_layout_path,
                                                   current_scene_path);
}

bool LineDrawingSceneCatalog_EntryMatchesQuery(const LineDrawingSceneCatalogEntry* entry,
                                               const char* query) {
    if (!entry) return false;
    if (!query || !query[0]) return true;
    return line_drawing_scene_catalog_contains_case_insensitive(entry->label, query) ||
           line_drawing_scene_catalog_contains_case_insensitive(entry->path, query) ||
           line_drawing_scene_catalog_contains_case_insensitive(
               LineDrawingFileCatalog_PathBasename(entry->path),
               query);
}

const LineDrawingSceneCatalogEntry* LineDrawingSceneCatalog_GetLayout(
    const LineDrawingSceneCatalog* catalog,
    int index) {
    if (!catalog || index < 0 || index >= catalog->layout_count) return NULL;
    return &catalog->layouts[index];
}

const LineDrawingSceneCatalogEntry* LineDrawingSceneCatalog_GetScene(
    const LineDrawingSceneCatalog* catalog,
    int index) {
    if (!catalog || index < 0 || index >= catalog->scene_count) return NULL;
    return &catalog->scenes[index];
}
