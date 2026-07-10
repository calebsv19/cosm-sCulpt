#include "Menu/line_drawing_root_browser.h"

#include "Core/data_paths.h"
#include "Core/line_drawing_file_catalog.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

enum {
    LINE_DRAWING_ROOT_BROWSER_MAX_CANDIDATES = 24
};

typedef struct LineDrawingRootBrowserCandidate {
    char path[MAX_CONFIG_PATH];
    char label[128];
    char description[160];
    int score;
    LineDrawingCatalogPreviewSourceKind preview_kind;
    char preview_path[MAX_CONFIG_PATH];
} LineDrawingRootBrowserCandidate;

static void line_drawing_root_browser_copy_text(char* dst, size_t dst_size, const char* src) {
    size_t len = 0u;
    if (!dst || dst_size == 0u) return;
    if (!src) src = "";
    len = strlen(src);
    if (len >= dst_size) {
        len = dst_size - 1u;
    }
    if (len > 0u) {
        memcpy(dst, src, len);
    }
    dst[len] = '\0';
}

static void line_drawing_root_browser_append_text(char* dst, size_t dst_size, const char* src) {
    size_t used = 0u;
    size_t remaining = 0u;
    if (!dst || dst_size == 0u || !src || !src[0]) return;
    used = strlen(dst);
    if (used >= dst_size - 1u) return;
    remaining = dst_size - used;
    line_drawing_root_browser_copy_text(dst + used, remaining, src);
}

static void line_drawing_root_browser_join_label(char* dst,
                                                 size_t dst_size,
                                                 const char* left,
                                                 const char* right) {
    line_drawing_root_browser_copy_text(dst, dst_size, left);
    line_drawing_root_browser_append_text(dst, dst_size, "/");
    line_drawing_root_browser_append_text(dst, dst_size, right);
}

static bool line_drawing_root_browser_is_scene_authoring_name(const char* name) {
    return name && strcasecmp(name, LineDrawingFileCatalog_SceneAuthoringFilename()) == 0;
}

static bool line_drawing_root_browser_resolve_preview_target(
    const char* directory_path,
    LineDrawingCatalogPreviewSourceKind* out_kind,
    char* out_path,
    size_t out_path_size) {
    DIR* dir = NULL;
    struct dirent* entry = NULL;
    char fallback_layout_path[MAX_CONFIG_PATH] = {0};
    bool found_layout = false;

    if (!directory_path || !directory_path[0] || !out_kind || !out_path || out_path_size == 0u) {
        return false;
    }

    if (LineDrawingFileCatalog_JoinPath(directory_path,
                                        LineDrawingFileCatalog_SceneAuthoringFilename(),
                                        out_path,
                                        out_path_size) &&
        LineDrawingFileCatalog_PathIsRegularFile(out_path)) {
        *out_kind = LINE_DRAWING_CATALOG_PREVIEW_SOURCE_SCENE;
        return true;
    }

    dir = opendir(directory_path);
    if (!dir) return false;
    while ((entry = readdir(dir)) != NULL) {
        char child_path[MAX_CONFIG_PATH];
        if (entry->d_name[0] == '.') continue;
        if (!LineDrawingFileCatalog_JoinPath(directory_path,
                                             entry->d_name,
                                             child_path,
                                             sizeof(child_path))) {
            continue;
        }
        if (LineDrawingFileCatalog_PathIsDirectory(child_path)) {
            char authoring_path[MAX_CONFIG_PATH];
            if (LineDrawingFileCatalog_JoinPath(child_path,
                                                LineDrawingFileCatalog_SceneAuthoringFilename(),
                                                authoring_path,
                                                sizeof(authoring_path)) &&
                LineDrawingFileCatalog_PathIsRegularFile(authoring_path)) {
                *out_kind = LINE_DRAWING_CATALOG_PREVIEW_SOURCE_SCENE;
                snprintf(out_path, out_path_size, "%s", authoring_path);
                closedir(dir);
                return true;
            }
            continue;
        }
        if (!LineDrawingFileCatalog_PathIsRegularFile(child_path) ||
            !LineDrawingFileCatalog_NameHasJsonSuffix(entry->d_name)) {
            continue;
        }
        if (line_drawing_root_browser_is_scene_authoring_name(entry->d_name)) {
            *out_kind = LINE_DRAWING_CATALOG_PREVIEW_SOURCE_SCENE;
            snprintf(out_path, out_path_size, "%s", child_path);
            closedir(dir);
            return true;
        }
        if (!found_layout) {
            snprintf(fallback_layout_path, sizeof(fallback_layout_path), "%s", child_path);
            found_layout = true;
        }
    }
    closedir(dir);

    if (found_layout) {
        *out_kind = LINE_DRAWING_CATALOG_PREVIEW_SOURCE_LAYOUT;
        snprintf(out_path, out_path_size, "%s", fallback_layout_path);
        return true;
    }
    return false;
}

static bool line_drawing_root_browser_contains_case_insensitive(const char* haystack,
                                                                const char* needle) {
    size_t hay_len = 0u;
    size_t needle_len = 0u;
    size_t i = 0u;
    if (!haystack || !needle) return false;
    hay_len = strlen(haystack);
    needle_len = strlen(needle);
    if (needle_len == 0u || hay_len < needle_len) return false;
    for (i = 0u; i + needle_len <= hay_len; ++i) {
        if (strncasecmp(haystack + i, needle, needle_len) == 0) {
            return true;
        }
    }
    return false;
}

static bool line_drawing_root_browser_name_has_keyword(const char* name) {
    static const char* keywords[] = {
        "scene", "layout", "author", "render", "room", "gallery", "block", "request"
    };
    size_t i = 0u;
    if (!name || !name[0]) return false;
    for (i = 0u; i < sizeof(keywords) / sizeof(keywords[0]); ++i) {
        if (line_drawing_root_browser_contains_case_insensitive(name, keywords[i])) {
            return true;
        }
    }
    return false;
}

static int line_drawing_root_browser_score_directory(const char* path,
                                                     const char* basename,
                                                     char* out_description,
                                                     size_t out_description_size) {
    DIR* dir = NULL;
    struct dirent* entry = NULL;
    int json_count = 0;
    int child_scene_dirs = 0;
    bool has_authoring = false;
    bool has_runtime = false;
    char feature_text[96] = {0};
    int score = 0;

    if (!path || !path[0]) return 0;
    dir = opendir(path);
    if (!dir) return 0;

    while ((entry = readdir(dir)) != NULL) {
        char child_path[MAX_CONFIG_PATH];
        if (entry->d_name[0] == '.') continue;
        if (!LineDrawingFileCatalog_JoinPath(path,
                                             entry->d_name,
                                             child_path,
                                             sizeof(child_path))) {
            continue;
        }
        if (LineDrawingFileCatalog_PathIsRegularFile(child_path) &&
            LineDrawingFileCatalog_NameHasJsonSuffix(entry->d_name)) {
            json_count += 1;
            if (strcasecmp(entry->d_name, LineDrawingFileCatalog_SceneAuthoringFilename()) == 0) {
                has_authoring = true;
            } else if (strcasecmp(entry->d_name, LineDrawingFileCatalog_SceneRuntimeFilename()) == 0) {
                has_runtime = true;
            }
        } else if (LineDrawingFileCatalog_PathIsDirectory(child_path)) {
            char scene_path[MAX_CONFIG_PATH];
            if (LineDrawingFileCatalog_JoinPath(child_path,
                                                LineDrawingFileCatalog_SceneAuthoringFilename(),
                                                scene_path,
                                                sizeof(scene_path)) &&
                LineDrawingFileCatalog_PathIsRegularFile(scene_path)) {
                child_scene_dirs += 1;
            }
        }
    }
    closedir(dir);

    if (has_authoring) score += 120;
    if (has_runtime) score += 80;
    score += (json_count > 6 ? 6 : json_count) * 8;
    score += (child_scene_dirs > 3 ? 3 : child_scene_dirs) * 24;
    if (line_drawing_root_browser_name_has_keyword(basename)) {
        score += 16;
    }

    if (out_description && out_description_size > 0u) {
        if (has_authoring) {
            snprintf(feature_text, sizeof(feature_text), "scene authoring root");
        } else if (child_scene_dirs > 0) {
            snprintf(feature_text,
                     sizeof(feature_text),
                     "%d child scene director%s",
                     child_scene_dirs,
                     child_scene_dirs == 1 ? "y" : "ies");
        } else if (json_count > 0) {
            snprintf(feature_text,
                     sizeof(feature_text),
                     "%d JSON file%s",
                     json_count,
                     json_count == 1 ? "" : "s");
        } else if (has_runtime) {
            snprintf(feature_text, sizeof(feature_text), "scene runtime files");
        } else {
            snprintf(feature_text, sizeof(feature_text), "nearby directory");
        }
        snprintf(out_description, out_description_size, "%s", feature_text);
    }

    return score;
}

static bool line_drawing_root_browser_add_entry(LineDrawingRootBrowser* browser,
                                                LineDrawingRootBrowserEntryKind kind,
                                                bool enabled,
                                                int score,
                                                LineDrawingCatalogPreviewSourceKind preview_kind,
                                                const char* label,
                                                const char* description,
                                                const char* path,
                                                const char* preview_path) {
    LineDrawingRootBrowserEntry* entry = NULL;
    if (!browser || browser->entry_count < 0 || browser->entry_count >= MAX_CONFIG_FILES) return false;
    entry = &browser->entries[browser->entry_count++];
    memset(entry, 0, sizeof(*entry));
    entry->kind = kind;
    entry->enabled = enabled;
    entry->score = score;
    entry->preview_kind = preview_kind;
    line_drawing_root_browser_copy_text(entry->label, sizeof(entry->label), label);
    line_drawing_root_browser_copy_text(entry->description, sizeof(entry->description), description);
    line_drawing_root_browser_copy_text(entry->path, sizeof(entry->path), path);
    line_drawing_root_browser_copy_text(entry->preview_path, sizeof(entry->preview_path), preview_path);
    return true;
}

static void line_drawing_root_browser_add_candidate(LineDrawingRootBrowserCandidate* candidates,
                                                    int* count,
                                                    const char* path,
                                                    const char* label,
                                                    const char* description,
                                                    int score,
                                                    LineDrawingCatalogPreviewSourceKind preview_kind,
                                                    const char* preview_path) {
    int i = 0;
    if (!candidates || !count || !path || !path[0] || !label || !label[0]) return;
    if (score <= 0) return;

    for (i = 0; i < *count; ++i) {
        if (strcasecmp(candidates[i].path, path) == 0) {
            if (score > candidates[i].score) {
                candidates[i].score = score;
                candidates[i].preview_kind = preview_kind;
                line_drawing_root_browser_copy_text(candidates[i].label,
                                                    sizeof(candidates[i].label),
                                                    label);
                line_drawing_root_browser_copy_text(candidates[i].description,
                                                    sizeof(candidates[i].description),
                                                    description);
                line_drawing_root_browser_copy_text(candidates[i].preview_path,
                                                    sizeof(candidates[i].preview_path),
                                                    preview_path);
            }
            return;
        }
    }

    if (*count >= LINE_DRAWING_ROOT_BROWSER_MAX_CANDIDATES) return;
    line_drawing_root_browser_copy_text(candidates[*count].path, sizeof(candidates[*count].path), path);
    line_drawing_root_browser_copy_text(candidates[*count].label, sizeof(candidates[*count].label), label);
    line_drawing_root_browser_copy_text(candidates[*count].description,
                                        sizeof(candidates[*count].description),
                                        description);
    candidates[*count].score = score;
    candidates[*count].preview_kind = preview_kind;
    line_drawing_root_browser_copy_text(candidates[*count].preview_path,
                                        sizeof(candidates[*count].preview_path),
                                        preview_path);
    *count += 1;
}

static void line_drawing_root_browser_sort_candidates(LineDrawingRootBrowserCandidate* candidates,
                                                      int count) {
    int i = 0;
    int j = 0;
    LineDrawingRootBrowserCandidate tmp;
    if (!candidates || count <= 1) return;
    for (i = 0; i < count - 1; ++i) {
        for (j = i + 1; j < count; ++j) {
            if (candidates[j].score > candidates[i].score ||
                (candidates[j].score == candidates[i].score &&
                 strcasecmp(candidates[j].label, candidates[i].label) < 0)) {
                tmp = candidates[i];
                candidates[i] = candidates[j];
                candidates[j] = tmp;
            }
        }
    }
}

static void line_drawing_root_browser_scan_siblings(LineDrawingRootBrowserCandidate* candidates,
                                                    int* count,
                                                    const char* current_path,
                                                    const char* parent_path) {
    DIR* dir = NULL;
    struct dirent* entry = NULL;
    if (!candidates || !count || !current_path || !parent_path || !parent_path[0]) return;

    dir = opendir(parent_path);
    if (!dir) return;
    while ((entry = readdir(dir)) != NULL) {
        char child_path[MAX_CONFIG_PATH];
        char description[320];
        char base_description[160];
        char preview_path[MAX_CONFIG_PATH] = {0};
        LineDrawingCatalogPreviewSourceKind preview_kind =
            LINE_DRAWING_CATALOG_PREVIEW_SOURCE_LAYOUT;
        int score = 0;
        if (entry->d_name[0] == '.') continue;
        if (!LineDrawingFileCatalog_JoinPath(parent_path,
                                             entry->d_name,
                                             child_path,
                                             sizeof(child_path))) {
            continue;
        }
        if (strcasecmp(child_path, current_path) == 0) continue;
        if (!LineDrawingFileCatalog_PathIsDirectory(child_path)) continue;

        score = line_drawing_root_browser_score_directory(child_path,
                                                          entry->d_name,
                                                          description,
                                                          sizeof(description));
        if (score <= 0) continue;
        line_drawing_root_browser_copy_text(base_description, sizeof(base_description), description);
        (void)line_drawing_root_browser_resolve_preview_target(child_path,
                                                               &preview_kind,
                                                               preview_path,
                                                               sizeof(preview_path));
        score += 28;
        snprintf(description,
                 sizeof(description),
                 "Sibling branch: %s",
                 base_description[0] ? base_description : "nearby directory");
        line_drawing_root_browser_add_candidate(candidates,
                                                count,
                                                child_path,
                                                entry->d_name,
                                                description,
                                                score,
                                                preview_kind,
                                                preview_path);
    }
    closedir(dir);
}

static void line_drawing_root_browser_scan_children(LineDrawingRootBrowserCandidate* candidates,
                                                    int* count,
                                                    const char* current_path) {
    DIR* dir = NULL;
    struct dirent* entry = NULL;
    if (!candidates || !count || !current_path || !current_path[0]) return;

    dir = opendir(current_path);
    if (!dir) return;
    while ((entry = readdir(dir)) != NULL) {
        char child_path[MAX_CONFIG_PATH];
        char description[320];
        char base_description[160];
        char preview_path[MAX_CONFIG_PATH] = {0};
        LineDrawingCatalogPreviewSourceKind preview_kind =
            LINE_DRAWING_CATALOG_PREVIEW_SOURCE_LAYOUT;
        int score = 0;
        if (entry->d_name[0] == '.') continue;
        if (!LineDrawingFileCatalog_JoinPath(current_path,
                                             entry->d_name,
                                             child_path,
                                             sizeof(child_path))) {
            continue;
        }
        if (!LineDrawingFileCatalog_PathIsDirectory(child_path)) continue;

        score = line_drawing_root_browser_score_directory(child_path,
                                                          entry->d_name,
                                                          description,
                                                          sizeof(description));
        if (score <= 0) continue;
        line_drawing_root_browser_copy_text(base_description, sizeof(base_description), description);
        (void)line_drawing_root_browser_resolve_preview_target(child_path,
                                                               &preview_kind,
                                                               preview_path,
                                                               sizeof(preview_path));
        score += 18;
        snprintf(description,
                 sizeof(description),
                 "Child branch: %s",
                 base_description[0] ? base_description : "nearby directory");
        line_drawing_root_browser_add_candidate(candidates,
                                                count,
                                                child_path,
                                                entry->d_name,
                                                description,
                                                score,
                                                preview_kind,
                                                preview_path);
    }
    closedir(dir);
}

static void line_drawing_root_browser_scan_cousins(LineDrawingRootBrowserCandidate* candidates,
                                                   int* count,
                                                   const char* parent_path,
                                                   const char* grandparent_path) {
    DIR* branch_dir = NULL;
    struct dirent* branch_entry = NULL;
    if (!candidates || !count || !parent_path || !grandparent_path || !grandparent_path[0]) return;

    branch_dir = opendir(grandparent_path);
    if (!branch_dir) return;
    while ((branch_entry = readdir(branch_dir)) != NULL) {
        char branch_path[MAX_CONFIG_PATH];
        DIR* child_dir = NULL;
        struct dirent* child_entry = NULL;

        if (branch_entry->d_name[0] == '.') continue;
        if (!LineDrawingFileCatalog_JoinPath(grandparent_path,
                                             branch_entry->d_name,
                                             branch_path,
                                             sizeof(branch_path))) {
            continue;
        }
        if (strcasecmp(branch_path, parent_path) == 0) continue;
        if (!LineDrawingFileCatalog_PathIsDirectory(branch_path)) continue;

        child_dir = opendir(branch_path);
        if (!child_dir) continue;
        while ((child_entry = readdir(child_dir)) != NULL) {
            char child_path[MAX_CONFIG_PATH];
            char description[320];
            char base_description[160];
            char label[256];
            char preview_path[MAX_CONFIG_PATH] = {0};
            LineDrawingCatalogPreviewSourceKind preview_kind =
                LINE_DRAWING_CATALOG_PREVIEW_SOURCE_LAYOUT;
            int score = 0;
            if (child_entry->d_name[0] == '.') continue;
            if (!LineDrawingFileCatalog_JoinPath(branch_path,
                                                 child_entry->d_name,
                                                 child_path,
                                                 sizeof(child_path))) {
                continue;
            }
            if (!LineDrawingFileCatalog_PathIsDirectory(child_path)) continue;
            score = line_drawing_root_browser_score_directory(child_path,
                                                              child_entry->d_name,
                                                              description,
                                                              sizeof(description));
            if (score <= 0) continue;
            line_drawing_root_browser_copy_text(base_description, sizeof(base_description), description);
            (void)line_drawing_root_browser_resolve_preview_target(child_path,
                                                                   &preview_kind,
                                                                   preview_path,
                                                                   sizeof(preview_path));
            score += 12;
            line_drawing_root_browser_join_label(label,
                                                 sizeof(label),
                                                 branch_entry->d_name,
                                                 child_entry->d_name);
            snprintf(description,
                     sizeof(description),
                     "Cousin branch: %s",
                     base_description[0] ? base_description : "nearby directory");
            line_drawing_root_browser_add_candidate(candidates,
                                                    count,
                                                    child_path,
                                                    label,
                                                    description,
                                                    score,
                                                    preview_kind,
                                                    preview_path);
        }
        closedir(child_dir);
    }
    closedir(branch_dir);
}

void LineDrawingRootBrowser_Init(LineDrawingRootBrowser* browser) {
    if (!browser) return;
    memset(browser, 0, sizeof(*browser));
}

void LineDrawingRootBrowser_Refresh(LineDrawingRootBrowser* browser,
                                    const char* browse_root,
                                    const char* input_root,
                                    const char* output_root) {
    char resolved_root[MAX_CONFIG_PATH];
    char parent_path[MAX_CONFIG_PATH];
    char grandparent_path[MAX_CONFIG_PATH];
    LineDrawingRootBrowserCandidate candidates[LINE_DRAWING_ROOT_BROWSER_MAX_CANDIDATES];
    int candidate_count = 0;
    int i = 0;
    const char* effective_root = browse_root;

    if (!browser) return;
    LineDrawingRootBrowser_Init(browser);

    if (!effective_root || !effective_root[0]) {
        effective_root = (input_root && input_root[0]) ? input_root : LineDrawingDataPaths_DefaultInputRoot();
    }
    snprintf(resolved_root, sizeof(resolved_root), "%s", effective_root);
    LineDrawingFileCatalog_NormalizePath(resolved_root);
    if (!LineDrawingFileCatalog_PathIsDirectory(resolved_root)) {
        snprintf(resolved_root, sizeof(resolved_root), "%s", LineDrawingDataPaths_DefaultInputRoot());
        LineDrawingFileCatalog_NormalizePath(resolved_root);
    }
    snprintf(browser->current_path, sizeof(browser->current_path), "%s", resolved_root);

    memset(candidates, 0, sizeof(candidates));
    line_drawing_root_browser_scan_children(candidates,
                                            &candidate_count,
                                            browser->current_path);
    if (LineDrawingFileCatalog_ParentPath(browser->current_path,
                                          parent_path,
                                          sizeof(parent_path))) {
        line_drawing_root_browser_scan_siblings(candidates,
                                                &candidate_count,
                                                browser->current_path,
                                                parent_path);
        if (LineDrawingFileCatalog_ParentPath(parent_path,
                                              grandparent_path,
                                              sizeof(grandparent_path))) {
            line_drawing_root_browser_scan_cousins(candidates,
                                                   &candidate_count,
                                                   parent_path,
                                                   grandparent_path);
        }
    }

    line_drawing_root_browser_sort_candidates(candidates, candidate_count);
    for (i = 0; i < candidate_count && browser->entry_count < MAX_CONFIG_FILES; ++i) {
        if (line_drawing_root_browser_add_entry(browser,
                                                LINE_DRAWING_ROOT_BROWSER_ENTRY_NEARBY_INPUT_ROOT,
                                                true,
                                                candidates[i].score,
                                                candidates[i].preview_kind,
                                                candidates[i].label,
                                                candidates[i].description,
                                                candidates[i].path,
                                                candidates[i].preview_path)) {
            browser->nearby_count += 1;
        }
    }
    (void)input_root;
    (void)output_root;
}

const LineDrawingRootBrowserEntry* LineDrawingRootBrowser_GetEntry(
    const LineDrawingRootBrowser* browser,
    int index) {
    if (!browser || index < 0 || index >= browser->entry_count) return NULL;
    return &browser->entries[index];
}
