#pragma once

#include "Core/line_drawing_file_catalog.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static inline bool ld_test_artifact_make_dir(const char* path) {
    if (!path || !path[0]) return false;
    return mkdir(path, 0700) == 0;
}

static inline char* ld_test_artifact_make_temp_dir(char* template_path) {
    if (!template_path || !template_path[0]) return NULL;
    return mkdtemp(template_path);
}

static inline bool ld_test_artifact_ensure_dir(const char* path) {
    if (!path || !path[0]) return false;
    if (mkdir(path, 0755) == 0) return true;
    return errno == EEXIST;
}

static inline bool ld_test_artifact_write_text_file(const char* path, const char* text) {
    FILE* file = NULL;
    size_t len = 0u;
    if (!path || !text) return false;
    file = fopen(path, "wb");
    if (!file) return false;
    len = strlen(text);
    if (fwrite(text, 1u, len, file) != len) {
        fclose(file);
        return false;
    }
    return fclose(file) == 0;
}

static inline bool ld_test_artifact_join_path(char* out,
                                              size_t out_size,
                                              const char* dir,
                                              const char* name) {
    return LineDrawingFileCatalog_JoinPath(dir, name, out, out_size);
}

static inline bool ld_test_artifact_scene_authoring_path(char* out,
                                                         size_t out_size,
                                                         const char* scene_dir) {
    return ld_test_artifact_join_path(out,
                                      out_size,
                                      scene_dir,
                                      LineDrawingFileCatalog_SceneAuthoringFilename());
}

static inline bool ld_test_artifact_scene_runtime_path(char* out,
                                                       size_t out_size,
                                                       const char* scene_dir) {
    return ld_test_artifact_join_path(out,
                                      out_size,
                                      scene_dir,
                                      LineDrawingFileCatalog_SceneRuntimeFilename());
}

static inline bool ld_test_artifact_write_scene_contract(const char* scene_dir,
                                                         const char* authoring_json,
                                                         const char* runtime_json) {
    char authoring_path[PATH_MAX];
    char runtime_path[PATH_MAX];
    if (!scene_dir || !scene_dir[0]) return false;
    if (!ld_test_artifact_ensure_dir(scene_dir)) return false;
    if (!ld_test_artifact_scene_authoring_path(authoring_path,
                                               sizeof(authoring_path),
                                               scene_dir)) {
        return false;
    }
    if (!ld_test_artifact_scene_runtime_path(runtime_path,
                                             sizeof(runtime_path),
                                             scene_dir)) {
        return false;
    }
    return ld_test_artifact_write_text_file(authoring_path,
                                            authoring_json ? authoring_json : "{}") &&
           ld_test_artifact_write_text_file(runtime_path,
                                            runtime_json ? runtime_json : "{}");
}

static inline void ld_test_artifact_clear_recent_context_files(void) {
    (void)unlink("data/runtime/recent_layouts.txt");
    (void)unlink("data/runtime/recent_scenes.txt");
    (void)unlink("data/runtime/recent_object_assets.txt");
    (void)unlink("data/runtime/recent_input_roots.txt");
    (void)unlink("data/runtime/recent_output_roots.txt");
}

static inline void ld_test_artifact_clear_runtime_state_files(void) {
    (void)unlink("data/runtime/file_browser_mode.txt");
    (void)unlink("data/runtime/file_browser_json_root.txt");
    (void)unlink("data/runtime/file_browser_scene_root.txt");
    (void)unlink("data/runtime/file_browser_object_root.txt");
    (void)unlink("data/runtime/file_browser_mesh_root.txt");
    (void)unlink("data/runtime/file_browser_stl_root.txt");
    (void)unlink("data/runtime/file_browser_last_json_entry.txt");
    (void)unlink("data/runtime/file_browser_last_scene_entry.txt");
    (void)unlink("data/runtime/file_browser_last_object_entry.txt");
    (void)unlink("data/runtime/file_browser_last_mesh_entry.txt");
    (void)unlink("data/runtime/file_browser_last_stl_entry.txt");
    ld_test_artifact_clear_recent_context_files();
    (void)unlink("data/runtime/input_root.txt");
    (void)unlink("data/runtime/output_root.txt");
    (void)unlink("data/runtime/layout_root.txt");
    (void)unlink("data/runtime/object_asset_root.txt");
}
