#include "Core/line_drawing_file_catalog.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

static const char* k_scene_authoring_filename = "scene_authoring.json";
static const char* k_scene_runtime_filename = "scene_runtime.json";
static const char* k_runtime_mesh_suffix = ".runtime.json";
static const char* k_stl_suffix = ".stl";

static bool line_drawing_file_catalog_path_is_directory(const char* path) {
    struct stat st = {0};
    if (!path || !path[0]) return false;
    if (stat(path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

bool LineDrawingFileCatalog_PathIsRegularFile(const char* path) {
    struct stat st = {0};
    if (!path || !path[0]) return false;
    if (stat(path, &st) != 0) return false;
    return S_ISREG(st.st_mode);
}

const char* LineDrawingFileCatalog_PathBasename(const char* path) {
    const char* slash = NULL;
    if (!path || !path[0]) return "";
    slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static bool line_drawing_file_catalog_add_entry(LineDrawingFileCatalogEntry* entries,
                                                int* count,
                                                int max_entries,
                                                const char* label,
                                                const char* full_path) {
    int i = 0;
    if (!entries || !count || max_entries <= 0 || !label || !label[0] || !full_path || !full_path[0]) {
        return false;
    }
    if (*count < 0 || *count >= max_entries) return false;

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

static bool line_drawing_file_catalog_compose_scene_paths(const char* scene_dir,
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

bool LineDrawingFileCatalog_DirectoryHasSceneContract(const char* scene_dir,
                                                      char* out_authoring_path,
                                                      size_t out_authoring_path_size) {
    char authoring_path[MAX_CONFIG_PATH];
    char runtime_path[MAX_CONFIG_PATH];
    if (!line_drawing_file_catalog_compose_scene_paths(scene_dir,
                                                       authoring_path,
                                                       sizeof(authoring_path),
                                                       runtime_path,
                                                       sizeof(runtime_path))) {
        return false;
    }
    if (!LineDrawingFileCatalog_PathIsRegularFile(authoring_path) ||
        !LineDrawingFileCatalog_PathIsRegularFile(runtime_path)) {
        return false;
    }
    if (out_authoring_path && out_authoring_path_size > 0u) {
        snprintf(out_authoring_path, out_authoring_path_size, "%s", authoring_path);
    }
    return true;
}

void LineDrawingFileCatalog_BuildSceneLabel(const char* group_name,
                                            const char* scene_dir,
                                            char* out_label,
                                            size_t out_label_size) {
    const char* scene_name = LineDrawingFileCatalog_PathBasename(scene_dir);
    if (!out_label || out_label_size == 0u) return;
    out_label[0] = '\0';

    if (group_name && group_name[0]) {
        snprintf(out_label, out_label_size, "%s/%s", group_name, scene_name);
        return;
    }
    snprintf(out_label, out_label_size, "%s", scene_name);
}

int LineDrawingFileCatalog_ScanLayoutEntries(LineDrawingFileCatalogEntry* entries,
                                             int max_entries,
                                             const char* root_dir) {
    DIR* dir = NULL;
    struct dirent* entry = NULL;
    int count = 0;

    if (!entries || max_entries <= 0 || !root_dir || !root_dir[0]) return 0;
    dir = opendir(root_dir);
    if (!dir) return 0;

    while ((entry = readdir(dir)) != NULL && count < max_entries) {
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
        if (!LineDrawingFileCatalog_PathIsRegularFile(full_path)) continue;
        (void)line_drawing_file_catalog_add_entry(entries, &count, max_entries, name, full_path);
    }

    closedir(dir);
    LineDrawingFileCatalog_SortEntries(entries, count);
    return count;
}

static void line_drawing_file_catalog_try_append_scene_dir(LineDrawingFileCatalogEntry* entries,
                                                           int* count,
                                                           int max_entries,
                                                           const char* scene_dir,
                                                           const char* group_name) {
    char authoring_path[MAX_CONFIG_PATH];
    char label[128];
    if (!entries || !count || !scene_dir || !scene_dir[0]) return;
    if (!LineDrawingFileCatalog_DirectoryHasSceneContract(scene_dir,
                                                          authoring_path,
                                                          sizeof(authoring_path))) {
        return;
    }
    LineDrawingFileCatalog_BuildSceneLabel(group_name, scene_dir, label, sizeof(label));
    (void)line_drawing_file_catalog_add_entry(entries, count, max_entries, label, authoring_path);
}

int LineDrawingFileCatalog_ScanSceneEntries(LineDrawingFileCatalogEntry* entries,
                                            int max_entries,
                                            const char* root_dir) {
    DIR* root = NULL;
    struct dirent* entry = NULL;
    int count = 0;

    if (!entries || max_entries <= 0 || !root_dir || !root_dir[0]) return 0;

    line_drawing_file_catalog_try_append_scene_dir(entries, &count, max_entries, root_dir, NULL);

    root = opendir(root_dir);
    if (!root) {
        LineDrawingFileCatalog_SortEntries(entries, count);
        return count;
    }

    while ((entry = readdir(root)) != NULL && count < max_entries) {
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
        if (!line_drawing_file_catalog_path_is_directory(candidate_path)) continue;
        if (LineDrawingFileCatalog_DirectoryHasSceneContract(candidate_path, NULL, 0u)) {
            line_drawing_file_catalog_try_append_scene_dir(entries,
                                                           &count,
                                                           max_entries,
                                                           candidate_path,
                                                           NULL);
            continue;
        }

        group_dir = opendir(candidate_path);
        if (!group_dir) continue;
        while ((grouped_entry = readdir(group_dir)) != NULL && count < max_entries) {
            char grouped_scene_path[MAX_CONFIG_PATH];
            if (grouped_entry->d_name[0] == '.') continue;
            if (snprintf(grouped_scene_path,
                         sizeof(grouped_scene_path),
                         "%s/%s",
                         candidate_path,
                         grouped_entry->d_name) >= (int)sizeof(grouped_scene_path)) {
                continue;
            }
            if (!line_drawing_file_catalog_path_is_directory(grouped_scene_path)) continue;
            line_drawing_file_catalog_try_append_scene_dir(entries,
                                                           &count,
                                                           max_entries,
                                                           grouped_scene_path,
                                                           entry->d_name);
        }
        closedir(group_dir);
    }

    closedir(root);
    LineDrawingFileCatalog_SortEntries(entries, count);
    return count;
}

static void line_drawing_file_catalog_scan_runtime_mesh_dir(LineDrawingFileCatalogEntry* entries,
                                                            int* count,
                                                            int max_entries,
                                                            const char* root_dir,
                                                            const char* group_name) {
    DIR* dir = NULL;
    struct dirent* entry = NULL;
    const size_t suffix_len = strlen(k_runtime_mesh_suffix);
    if (!entries || !count || *count >= max_entries || !root_dir || !root_dir[0]) return;
    dir = opendir(root_dir);
    if (!dir) return;

    while ((entry = readdir(dir)) != NULL && *count < max_entries) {
        char full_path[MAX_CONFIG_PATH];
        char label[128];
        const char* name = entry->d_name;
        size_t len = 0u;

        if (name[0] == '.') continue;
        len = strlen(name);
        if (len <= suffix_len) continue;
        if (strcasecmp(name + len - suffix_len, k_runtime_mesh_suffix) != 0) continue;
        if (snprintf(full_path, sizeof(full_path), "%s/%s", root_dir, name) >= (int)sizeof(full_path)) {
            continue;
        }
        if (!LineDrawingFileCatalog_PathIsRegularFile(full_path)) continue;
        if (group_name && group_name[0]) {
            snprintf(label, sizeof(label), "%s/%s", group_name, name);
        } else {
            snprintf(label, sizeof(label), "%s", name);
        }
        (void)line_drawing_file_catalog_add_entry(entries, count, max_entries, label, full_path);
    }

    closedir(dir);
}

int LineDrawingFileCatalog_ScanRuntimeMeshEntries(LineDrawingFileCatalogEntry* entries,
                                                  int max_entries,
                                                  const char* root_dir) {
    DIR* root = NULL;
    struct dirent* entry = NULL;
    int count = 0;

    if (!entries || max_entries <= 0 || !root_dir || !root_dir[0]) return 0;

    line_drawing_file_catalog_scan_runtime_mesh_dir(entries,
                                                    &count,
                                                    max_entries,
                                                    root_dir,
                                                    NULL);

    root = opendir(root_dir);
    if (!root) {
        LineDrawingFileCatalog_SortEntries(entries, count);
        return count;
    }

    while ((entry = readdir(root)) != NULL && count < max_entries) {
        char candidate_path[MAX_CONFIG_PATH];
        if (entry->d_name[0] == '.') continue;
        if (snprintf(candidate_path,
                     sizeof(candidate_path),
                     "%s/%s",
                     root_dir,
                     entry->d_name) >= (int)sizeof(candidate_path)) {
            continue;
        }
        if (!line_drawing_file_catalog_path_is_directory(candidate_path)) continue;
        line_drawing_file_catalog_scan_runtime_mesh_dir(entries,
                                                        &count,
                                                        max_entries,
                                                        candidate_path,
                                                        entry->d_name);
    }

    closedir(root);
    LineDrawingFileCatalog_SortEntries(entries, count);
    return count;
}

static void line_drawing_file_catalog_scan_stl_dir(LineDrawingFileCatalogEntry* entries,
                                                   int* count,
                                                   int max_entries,
                                                   const char* root_dir,
                                                   const char* group_name) {
    DIR* dir = NULL;
    struct dirent* entry = NULL;
    const size_t suffix_len = strlen(k_stl_suffix);
    if (!entries || !count || *count >= max_entries || !root_dir || !root_dir[0]) return;
    dir = opendir(root_dir);
    if (!dir) return;

    while ((entry = readdir(dir)) != NULL && *count < max_entries) {
        char full_path[MAX_CONFIG_PATH];
        char label[128];
        const char* name = entry->d_name;
        size_t len = 0u;

        if (name[0] == '.') continue;
        len = strlen(name);
        if (len <= suffix_len) continue;
        if (strcasecmp(name + len - suffix_len, k_stl_suffix) != 0) continue;
        if (snprintf(full_path, sizeof(full_path), "%s/%s", root_dir, name) >=
            (int)sizeof(full_path)) {
            continue;
        }
        if (!LineDrawingFileCatalog_PathIsRegularFile(full_path)) continue;
        if (group_name && group_name[0]) {
            snprintf(label, sizeof(label), "%s/%s", group_name, name);
        } else {
            snprintf(label, sizeof(label), "%s", name);
        }
        (void)line_drawing_file_catalog_add_entry(entries, count, max_entries, label, full_path);
    }

    closedir(dir);
}

int LineDrawingFileCatalog_ScanStlEntries(LineDrawingFileCatalogEntry* entries,
                                          int max_entries,
                                          const char* root_dir) {
    DIR* root = NULL;
    struct dirent* entry = NULL;
    int count = 0;

    if (!entries || max_entries <= 0 || !root_dir || !root_dir[0]) return 0;

    line_drawing_file_catalog_scan_stl_dir(entries, &count, max_entries, root_dir, NULL);

    root = opendir(root_dir);
    if (!root) {
        LineDrawingFileCatalog_SortEntries(entries, count);
        return count;
    }

    while ((entry = readdir(root)) != NULL && count < max_entries) {
        char candidate_path[MAX_CONFIG_PATH];
        if (entry->d_name[0] == '.') continue;
        if (snprintf(candidate_path,
                     sizeof(candidate_path),
                     "%s/%s",
                     root_dir,
                     entry->d_name) >= (int)sizeof(candidate_path)) {
            continue;
        }
        if (!line_drawing_file_catalog_path_is_directory(candidate_path)) continue;
        line_drawing_file_catalog_scan_stl_dir(entries,
                                               &count,
                                               max_entries,
                                               candidate_path,
                                               entry->d_name);
    }

    closedir(root);
    LineDrawingFileCatalog_SortEntries(entries, count);
    return count;
}

void LineDrawingFileCatalog_SortEntries(LineDrawingFileCatalogEntry* entries, int count) {
    int i = 0;
    int j = 0;
    LineDrawingFileCatalogEntry tmp;
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
