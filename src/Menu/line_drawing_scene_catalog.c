#include "Menu/line_drawing_scene_catalog.h"

#include <dirent.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

static const char* k_scene_authoring_filename = "scene_authoring.json";
static const char* k_scene_runtime_filename = "scene_runtime.json";

static bool line_drawing_scene_catalog_path_is_directory(const char* path) {
    struct stat st = {0};
    if (!path || !path[0]) return false;
    if (stat(path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

static bool line_drawing_scene_catalog_path_is_regular_file(const char* path) {
    struct stat st = {0};
    if (!path || !path[0]) return false;
    if (stat(path, &st) != 0) return false;
    return S_ISREG(st.st_mode);
}

static const char* line_drawing_scene_catalog_path_basename(const char* path) {
    const char* slash = NULL;
    if (!path || !path[0]) return "";
    slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

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

static bool line_drawing_scene_catalog_add_entry(LineDrawingSceneCatalogEntry* entries,
                                                 int* count,
                                                 const char* label,
                                                 const char* full_path) {
    int i = 0;
    if (!entries || !count || !label || !label[0] || !full_path || !full_path[0]) {
        return false;
    }
    if (*count < 0 || *count >= MAX_CONFIG_FILES) return false;

    for (i = 0; i < *count; ++i) {
        if (strcasecmp(entries[i].path, full_path) == 0) {
            return false;
        }
    }

    snprintf(entries[*count].label, sizeof(entries[*count].label), "%s", label);
    snprintf(entries[*count].path, sizeof(entries[*count].path), "%s", full_path);
    *count += 1;
    return true;
}

static void line_drawing_scene_catalog_sort(LineDrawingSceneCatalogEntry* entries, int count) {
    int i = 0;
    int j = 0;
    LineDrawingSceneCatalogEntry tmp;
    if (!entries || count <= 1) return;
    for (i = 0; i < count - 1; ++i) {
        for (j = i + 1; j < count; ++j) {
            if (strcasecmp(entries[j].label, entries[i].label) < 0) {
                tmp = entries[i];
                entries[i] = entries[j];
                entries[j] = tmp;
            }
        }
    }
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

static bool line_drawing_scene_catalog_compose_scene_paths(const char* scene_dir,
                                                           char* out_authoring_path,
                                                           size_t out_authoring_path_size,
                                                           char* out_runtime_path,
                                                           size_t out_runtime_path_size) {
    if (!scene_dir || !scene_dir[0]) return false;
    if (!out_authoring_path || out_authoring_path_size == 0u) return false;
    if (!out_runtime_path || out_runtime_path_size == 0u) return false;

    if (snprintf(out_authoring_path,
                 out_authoring_path_size,
                 "%s/%s",
                 scene_dir,
                 k_scene_authoring_filename) >= (int)out_authoring_path_size) {
        return false;
    }
    if (snprintf(out_runtime_path,
                 out_runtime_path_size,
                 "%s/%s",
                 scene_dir,
                 k_scene_runtime_filename) >= (int)out_runtime_path_size) {
        return false;
    }
    return true;
}

static bool line_drawing_scene_catalog_directory_has_scene_contract(
    const char* scene_dir,
    char* out_authoring_path,
    size_t out_authoring_path_size) {
    char authoring_path[MAX_CONFIG_PATH];
    char runtime_path[MAX_CONFIG_PATH];
    if (!line_drawing_scene_catalog_compose_scene_paths(scene_dir,
                                                        authoring_path,
                                                        sizeof(authoring_path),
                                                        runtime_path,
                                                        sizeof(runtime_path))) {
        return false;
    }
    if (!line_drawing_scene_catalog_path_is_regular_file(authoring_path) ||
        !line_drawing_scene_catalog_path_is_regular_file(runtime_path)) {
        return false;
    }
    if (out_authoring_path && out_authoring_path_size > 0u) {
        snprintf(out_authoring_path, out_authoring_path_size, "%s", authoring_path);
    }
    return true;
}

static void line_drawing_scene_catalog_build_scene_label(const char* group_name,
                                                         const char* scene_dir,
                                                         char* out_label,
                                                         size_t out_label_size) {
    const char* scene_name = line_drawing_scene_catalog_path_basename(scene_dir);
    if (!out_label || out_label_size == 0u) return;
    out_label[0] = '\0';

    if (group_name && group_name[0]) {
        snprintf(out_label, out_label_size, "%s/%s", group_name, scene_name);
        return;
    }
    snprintf(out_label, out_label_size, "%s", scene_name);
}

static void line_drawing_scene_catalog_scan_json_directory(LineDrawingSceneCatalog* catalog,
                                                           const char* root_dir) {
    DIR* dir = NULL;
    struct dirent* entry = NULL;

    if (!catalog || !root_dir || !root_dir[0]) return;
    dir = opendir(root_dir);
    if (!dir) return;

    while ((entry = readdir(dir)) != NULL && catalog->layout_count < MAX_CONFIG_FILES) {
        char full_path[MAX_CONFIG_PATH];
        const char* name = entry->d_name;
        size_t len = 0u;

        if (name[0] == '.') continue;
        len = strlen(name);
        if (len < 5u) continue;
        if (strcasecmp(name + len - 5u, ".json") != 0) continue;
        if (snprintf(full_path, sizeof(full_path), "%s/%s", root_dir, name) >= (int)sizeof(full_path)) {
            continue;
        }
        if (!line_drawing_scene_catalog_path_is_regular_file(full_path)) continue;
        (void)line_drawing_scene_catalog_add_entry(catalog->layouts,
                                                   &catalog->layout_count,
                                                   name,
                                                   full_path);
    }

    closedir(dir);
}

static void line_drawing_scene_catalog_try_append_scene_dir(LineDrawingSceneCatalog* catalog,
                                                            const char* scene_dir,
                                                            const char* group_name) {
    char authoring_path[MAX_CONFIG_PATH];
    char label[128];
    if (!catalog || !scene_dir || !scene_dir[0]) return;
    if (!line_drawing_scene_catalog_directory_has_scene_contract(scene_dir,
                                                                 authoring_path,
                                                                 sizeof(authoring_path))) {
        return;
    }
    line_drawing_scene_catalog_build_scene_label(group_name, scene_dir, label, sizeof(label));
    (void)line_drawing_scene_catalog_add_entry(catalog->scenes,
                                               &catalog->scene_count,
                                               label,
                                               authoring_path);
}

static void line_drawing_scene_catalog_scan_scene_root(LineDrawingSceneCatalog* catalog,
                                                       const char* root_dir) {
    DIR* root = NULL;
    struct dirent* entry = NULL;

    if (!catalog || !root_dir || !root_dir[0]) return;

    line_drawing_scene_catalog_try_append_scene_dir(catalog, root_dir, NULL);

    root = opendir(root_dir);
    if (!root) return;

    while ((entry = readdir(root)) != NULL && catalog->scene_count < MAX_CONFIG_FILES) {
        char candidate_path[MAX_CONFIG_PATH];
        DIR* group_dir = NULL;
        struct dirent* grouped_entry = NULL;

        if (entry->d_name[0] == '.') continue;
        if (snprintf(candidate_path,
                     sizeof(candidate_path),
                     "%s/%s",
                     root_dir,
                     entry->d_name) >= (int)sizeof(candidate_path)) {
            continue;
        }
        if (!line_drawing_scene_catalog_path_is_directory(candidate_path)) continue;
        if (line_drawing_scene_catalog_directory_has_scene_contract(candidate_path, NULL, 0u)) {
            line_drawing_scene_catalog_try_append_scene_dir(catalog, candidate_path, NULL);
            continue;
        }

        group_dir = opendir(candidate_path);
        if (!group_dir) continue;
        while ((grouped_entry = readdir(group_dir)) != NULL &&
               catalog->scene_count < MAX_CONFIG_FILES) {
            char grouped_scene_path[MAX_CONFIG_PATH];
            if (grouped_entry->d_name[0] == '.') continue;
            if (snprintf(grouped_scene_path,
                         sizeof(grouped_scene_path),
                         "%s/%s",
                         candidate_path,
                         grouped_entry->d_name) >= (int)sizeof(grouped_scene_path)) {
                continue;
            }
            if (!line_drawing_scene_catalog_path_is_directory(grouped_scene_path)) continue;
            line_drawing_scene_catalog_try_append_scene_dir(catalog,
                                                            grouped_scene_path,
                                                            entry->d_name);
        }
        closedir(group_dir);
    }

    closedir(root);
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

    line_drawing_scene_catalog_scan_json_directory(catalog, input_root);
    line_drawing_scene_catalog_scan_scene_root(catalog, input_root);
    line_drawing_scene_catalog_sort(catalog->layouts, catalog->layout_count);
    line_drawing_scene_catalog_sort(catalog->scenes, catalog->scene_count);
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
               line_drawing_scene_catalog_path_basename(entry->path),
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
