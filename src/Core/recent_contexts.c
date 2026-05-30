#include "Core/recent_contexts.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char* k_runtime_recent_layouts_path = "data/runtime/recent_layouts.txt";
static const char* k_runtime_recent_scenes_path = "data/runtime/recent_scenes.txt";
static const char* k_runtime_recent_object_assets_path = "data/runtime/recent_object_assets.txt";
static const char* k_runtime_recent_input_roots_path = "data/runtime/recent_input_roots.txt";
static const char* k_runtime_recent_output_roots_path = "data/runtime/recent_output_roots.txt";

static bool line_drawing_recent_contexts_ensure_runtime_dir(void) {
    if (mkdir("data", 0755) != 0 && errno != EEXIST) {
        return false;
    }
    if (mkdir("data/runtime", 0755) != 0 && errno != EEXIST) {
        return false;
    }
    return true;
}

static void line_drawing_recent_contexts_trim(char* text) {
    size_t start = 0u;
    size_t end = 0u;
    size_t len = 0u;
    if (!text) return;

    len = strlen(text);
    while (start < len && isspace((unsigned char)text[start])) {
        ++start;
    }
    end = len;
    while (end > start && isspace((unsigned char)text[end - 1u])) {
        --end;
    }
    if (start > 0u || end < len) {
        const size_t out_len = end - start;
        memmove(text, text + start, out_len);
        text[out_len] = '\0';
    }
}

static bool line_drawing_recent_contexts_push_front(LineDrawingRecentPathList* list,
                                                    const char* path) {
    int i = 0;
    int found_index = -1;
    char normalized[LINE_DRAWING_PATH_CAP];
    if (!list || !path || !path[0]) return false;

    snprintf(normalized, sizeof(normalized), "%s", path);
    line_drawing_recent_contexts_trim(normalized);
    if (!normalized[0]) return false;

    for (i = 0; i < list->count; ++i) {
        if (strcmp(list->paths[i], normalized) == 0) {
            found_index = i;
            break;
        }
    }

    if (found_index == 0) return false;

    if (found_index > 0) {
        for (i = found_index; i > 0; --i) {
            memcpy(list->paths[i], list->paths[i - 1], sizeof(list->paths[i]));
        }
    } else {
        if (list->count < LINE_DRAWING_RECENT_CONTEXT_LIMIT) {
            list->count += 1;
        }
        for (i = list->count - 1; i > 0; --i) {
            memcpy(list->paths[i], list->paths[i - 1], sizeof(list->paths[i]));
        }
    }

    snprintf(list->paths[0], sizeof(list->paths[0]), "%s", normalized);
    return true;
}

static bool line_drawing_recent_contexts_read_list(const char* file_path,
                                                   LineDrawingRecentPathList* out_list) {
    FILE* file = NULL;
    char line[LINE_DRAWING_PATH_CAP];
    if (!file_path || !out_list) return false;

    file = fopen(file_path, "rb");
    if (!file) return false;

    while (fgets(line, sizeof(line), file) != NULL) {
        (void)line_drawing_recent_contexts_push_front(out_list, line);
    }
    fclose(file);

    {
        int left = 0;
        int right = out_list->count - 1;
        while (left < right) {
            char tmp[LINE_DRAWING_PATH_CAP];
            memcpy(tmp, out_list->paths[left], sizeof(tmp));
            memcpy(out_list->paths[left], out_list->paths[right], sizeof(out_list->paths[left]));
            memcpy(out_list->paths[right], tmp, sizeof(out_list->paths[right]));
            ++left;
            --right;
        }
    }

    return true;
}

static bool line_drawing_recent_contexts_write_list(const char* file_path,
                                                    const LineDrawingRecentPathList* list) {
    FILE* file = NULL;
    int i = 0;
    if (!file_path || !list) return false;

    file = fopen(file_path, "wb");
    if (!file) return false;
    for (i = 0; i < list->count; ++i) {
        if (list->paths[i][0] == '\0') continue;
        if (fputs(list->paths[i], file) == EOF || fputc('\n', file) == EOF) {
            fclose(file);
            return false;
        }
    }
    fclose(file);
    return true;
}

void LineDrawingRecentContexts_Init(LineDrawingRecentContexts* contexts) {
    if (!contexts) return;
    memset(contexts, 0, sizeof(*contexts));
}

bool LineDrawingRecentContexts_Load(LineDrawingRecentContexts* contexts) {
    if (!contexts) return false;
    LineDrawingRecentContexts_Init(contexts);
    (void)line_drawing_recent_contexts_read_list(k_runtime_recent_layouts_path, &contexts->layouts);
    (void)line_drawing_recent_contexts_read_list(k_runtime_recent_scenes_path, &contexts->scenes);
    (void)line_drawing_recent_contexts_read_list(k_runtime_recent_object_assets_path,
                                                 &contexts->object_assets);
    (void)line_drawing_recent_contexts_read_list(k_runtime_recent_input_roots_path,
                                                 &contexts->input_roots);
    (void)line_drawing_recent_contexts_read_list(k_runtime_recent_output_roots_path,
                                                 &contexts->output_roots);
    return true;
}

bool LineDrawingRecentContexts_Save(const LineDrawingRecentContexts* contexts) {
    bool ok = true;
    if (!contexts) return false;
    if (!line_drawing_recent_contexts_ensure_runtime_dir()) return false;
    ok &= line_drawing_recent_contexts_write_list(k_runtime_recent_layouts_path, &contexts->layouts);
    ok &= line_drawing_recent_contexts_write_list(k_runtime_recent_scenes_path, &contexts->scenes);
    ok &= line_drawing_recent_contexts_write_list(k_runtime_recent_object_assets_path,
                                                  &contexts->object_assets);
    ok &= line_drawing_recent_contexts_write_list(k_runtime_recent_input_roots_path,
                                                  &contexts->input_roots);
    ok &= line_drawing_recent_contexts_write_list(k_runtime_recent_output_roots_path,
                                                  &contexts->output_roots);
    return ok;
}

bool LineDrawingRecentContexts_TrackLayout(LineDrawingRecentContexts* contexts, const char* path) {
    if (!contexts) return false;
    return line_drawing_recent_contexts_push_front(&contexts->layouts, path);
}

bool LineDrawingRecentContexts_TrackScene(LineDrawingRecentContexts* contexts, const char* path) {
    if (!contexts) return false;
    return line_drawing_recent_contexts_push_front(&contexts->scenes, path);
}

bool LineDrawingRecentContexts_TrackObjectAsset(LineDrawingRecentContexts* contexts, const char* path) {
    if (!contexts) return false;
    return line_drawing_recent_contexts_push_front(&contexts->object_assets, path);
}

bool LineDrawingRecentContexts_TrackInputRoot(LineDrawingRecentContexts* contexts, const char* path) {
    if (!contexts) return false;
    return line_drawing_recent_contexts_push_front(&contexts->input_roots, path);
}

bool LineDrawingRecentContexts_TrackOutputRoot(LineDrawingRecentContexts* contexts, const char* path) {
    if (!contexts) return false;
    return line_drawing_recent_contexts_push_front(&contexts->output_roots, path);
}
