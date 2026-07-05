#include "Tools/scene_export.h"

#include "Tools/canonical_scene_export.h"
#include "Tools/scene_project_export.h"
#include "Tools/shape_export.h"
#include "core_scene_compile.h"

#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

static const char* k_default_scene_stem = "layout_export";
static const char* k_authoring_filename = "scene_authoring.json";
static const char* k_runtime_filename = "scene_runtime.json";
static const char* k_default_scene_id = "scene_line_drawing";

static void write_diagnostics(char* diagnostics, size_t diagnostics_size, const char* message) {
    if (!diagnostics || diagnostics_size == 0) return;
    if (!message) {
        diagnostics[0] = '\0';
        return;
    }
    snprintf(diagnostics, diagnostics_size, "%s", message);
}

static const char* extract_basename(const char* path) {
    const char* slash = NULL;
    const char* backslash = NULL;
    const char* base = NULL;
    if (!path || !path[0]) return NULL;
    slash = strrchr(path, '/');
    backslash = strrchr(path, '\\');
    if (slash && backslash) {
        base = (slash > backslash) ? slash + 1 : backslash + 1;
    } else if (slash) {
        base = slash + 1;
    } else if (backslash) {
        base = backslash + 1;
    } else {
        base = path;
    }
    return (base && base[0]) ? base : NULL;
}

static bool extract_canonical_scene_parent_stem(const char* path,
                                                char* out_stem,
                                                size_t out_stem_size) {
    char directory[SHAPE_EXPORT_PATH_MAX];
    const char* base = NULL;
    const char* slash = NULL;
    size_t len = 0u;
    if (!path || !path[0] || !out_stem || out_stem_size == 0u) return false;
    base = extract_basename(path);
    if (!base || (strcasecmp(base, k_authoring_filename) != 0 &&
                  strcasecmp(base, k_runtime_filename) != 0)) {
        return false;
    }

    slash = strrchr(path, '/');
    if (!slash || slash == path) return false;
    len = (size_t)(slash - path);
    if (len == 0u || len >= sizeof(directory)) return false;
    memcpy(directory, path, len);
    directory[len] = '\0';

    base = extract_basename(directory);
    if (!base || !base[0]) return false;
    snprintf(out_stem, out_stem_size, "%s", base);
    return out_stem[0] != '\0';
}

static void derive_scene_stem(const char* layout_path_hint, char* out_stem, size_t out_stem_size) {
    const char* base = extract_basename(layout_path_hint);
    size_t len = 0;
    if (!out_stem || out_stem_size == 0) return;
    out_stem[0] = '\0';

    if (extract_canonical_scene_parent_stem(layout_path_hint, out_stem, out_stem_size)) {
        return;
    }

    if (!base || !base[0]) base = k_default_scene_stem;
    len = strlen(base);
    if (len >= 5 && strcasecmp(base + len - 5, ".json") == 0) {
        len -= 5;
    }
    if (len == 0) {
        snprintf(out_stem, out_stem_size, "%s", k_default_scene_stem);
        return;
    }
    if (len >= out_stem_size) len = out_stem_size - 1;
    memcpy(out_stem, base, len);
    out_stem[len] = '\0';
}

static void build_scene_id_from_stem(const char* stem, char* out_scene_id, size_t out_scene_id_size) {
    size_t prefix_len = 0;
    size_t out_len = 0;
    if (!out_scene_id || out_scene_id_size == 0) return;
    snprintf(out_scene_id, out_scene_id_size, "%s", k_default_scene_id);
    if (!stem || !stem[0]) return;

    prefix_len = strlen(k_default_scene_id);
    if (prefix_len + 2 >= out_scene_id_size) return;
    out_len = prefix_len;
    out_scene_id[out_len++] = '_';
    for (const char* p = stem; *p && out_len + 1 < out_scene_id_size; ++p) {
        unsigned char c = (unsigned char)*p;
        if (isalnum(c) || c == '_' || c == '-' || c == '.') {
            out_scene_id[out_len++] = (char)c;
        } else {
            out_scene_id[out_len++] = '_';
        }
    }
    out_scene_id[out_len] = '\0';
}

static bool ensure_directory_exists(const char* path) {
    struct stat st;
    if (!path || !path[0]) return false;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    if (mkdir(path, 0755) == 0) {
        return true;
    }
    return errno == EEXIST;
}

static bool ensure_directory_exists_with_created(const char* path, bool* out_created) {
    struct stat st;
    if (out_created) *out_created = false;
    if (!path || !path[0]) return false;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    if (mkdir(path, 0755) == 0) {
        if (out_created) *out_created = true;
        return true;
    }
    return errno == EEXIST;
}

static void cleanup_failed_new_scene_dir(const char* scene_dir,
                                         const char* authoring_path,
                                         const char* runtime_path,
                                         bool created_scene_dir) {
    if (!created_scene_dir) return;
    if (authoring_path && authoring_path[0]) (void)remove(authoring_path);
    if (runtime_path && runtime_path[0]) (void)remove(runtime_path);
    if (scene_dir && scene_dir[0]) (void)rmdir(scene_dir);
}

static bool build_path(const char* root,
                       const char* leaf,
                       char* out_path,
                       size_t out_path_size) {
    const size_t root_len = root ? strlen(root) : 0;
    const bool needs_slash = root_len > 0 && root[root_len - 1] != '/';
    int written = 0;
    if (!root || !root[0] || !leaf || !leaf[0] || !out_path || out_path_size == 0) {
        return false;
    }
    written = snprintf(out_path, out_path_size, "%s%s%s", root, needs_slash ? "/" : "", leaf);
    return written > 0 && (size_t)written < out_path_size;
}

static bool extract_directory_path(const char* path, char* out_dir, size_t out_dir_size) {
    const char* slash = NULL;
    size_t len = 0u;
    if (!path || !path[0] || !out_dir || out_dir_size == 0u) return false;
    slash = strrchr(path, '/');
    if (!slash) return false;
    len = (size_t)(slash - path);
    if (len == 0u || len >= out_dir_size) return false;
    memcpy(out_dir, path, len);
    out_dir[len] = '\0';
    return true;
}

static const char* extract_parent_basename(const char* path) {
    static char parent_name[128];
    char directory[SHAPE_EXPORT_PATH_MAX];
    const char* base = NULL;
    if (!extract_directory_path(path, directory, sizeof(directory))) return NULL;
    base = extract_basename(directory);
    if (!base || !base[0]) return NULL;
    snprintf(parent_name, sizeof(parent_name), "%s", base);
    return parent_name;
}

static size_t count_project_manifest_mesh_objects(const Layout* layout) {
    size_t count = 0u;
    if (!layout) return 0u;
    for (size_t i = 0u; i < layout->objectStore.count; ++i) {
        const Object3D* object = &layout->objectStore.items[i];
        if (object->isDeleted || object->kind != OBJECT3D_KIND_MESH_ASSET_INSTANCE) continue;
        if (!Layout_ObjectStore_ValidateObject(object)) continue;
        ++count;
    }
    return count;
}

static bool fill_project_manifest_mesh_objects(const Layout* layout,
                                               LineDrawingSceneProjectManifestObject* objects,
                                               size_t object_count) {
    size_t out = 0u;
    if (!layout || (!objects && object_count > 0u)) return false;
    for (size_t i = 0u; i < layout->objectStore.count; ++i) {
        const Object3D* object = &layout->objectStore.items[i];
        LineDrawingSceneProjectManifestObject* entry = NULL;
        if (object->isDeleted || object->kind != OBJECT3D_KIND_MESH_ASSET_INSTANCE) continue;
        if (!Layout_ObjectStore_ValidateObject(object)) continue;
        if (out >= object_count) return false;
        entry = &objects[out++];
        entry->object_id = object->coreMeta.object_id;
        entry->display_name = object->meshInstance.assetId;
        entry->kind = "mesh_asset_instance";
        entry->mesh_asset_id = object->meshInstance.assetId;
        entry->source_asset_id = object->meshInstance.sourceAssetId;
        entry->source_mesh_sidecar_path = object->meshInstance.runtimePath;
        entry->vertex_count = object->meshInstance.vertexCount;
        entry->triangle_count = object->meshInstance.triangleCount;
        entry->has_physics_extension = false;
        entry->has_ray_tracing_extension = false;
    }
    return out == object_count;
}

bool LineDrawingSceneExport_ExportLayoutToOutputRoot(const Layout* layout,
                                                     const char* layout_path_hint,
                                                     const char* output_root,
                                                     LineDrawingSceneExportPaths* out_paths,
                                                     char* diagnostics,
                                                     size_t diagnostics_size) {
    char stem[128];
    char scene_dir[SHAPE_EXPORT_PATH_MAX];
    char authoring_path[SHAPE_EXPORT_PATH_MAX];
    char runtime_path[SHAPE_EXPORT_PATH_MAX];
    char scene_id[64];
    CoreResult compile_result = {0};
    const char* root = output_root;
    bool created_scene_dir = false;

    write_diagnostics(diagnostics, diagnostics_size, NULL);
    scene_dir[0] = '\0';
    authoring_path[0] = '\0';
    runtime_path[0] = '\0';
    if (!layout) {
        write_diagnostics(diagnostics, diagnostics_size, "layout missing");
        return false;
    }
    if (!root || !root[0]) root = ShapeExport_GetExportDir();

    derive_scene_stem(layout_path_hint, stem, sizeof(stem));
    build_scene_id_from_stem(stem, scene_id, sizeof(scene_id));
    if (!build_path(root, stem, scene_dir, sizeof(scene_dir))) {
        write_diagnostics(diagnostics, diagnostics_size, "failed to build scene directory path");
        return false;
    }
    if (!ensure_directory_exists_with_created(scene_dir, &created_scene_dir)) {
        write_diagnostics(diagnostics, diagnostics_size, "failed to create scene directory");
        return false;
    }
    if (!ShapeExport_BuildPathInRoot(scene_dir, k_authoring_filename, authoring_path, sizeof(authoring_path))) {
        write_diagnostics(diagnostics, diagnostics_size, "failed to prepare authoring export path");
        cleanup_failed_new_scene_dir(scene_dir, authoring_path, runtime_path, created_scene_dir);
        return false;
    }
    if (!ShapeExport_BuildPathInRoot(scene_dir, k_runtime_filename, runtime_path, sizeof(runtime_path))) {
        write_diagnostics(diagnostics, diagnostics_size, "failed to prepare runtime export path");
        cleanup_failed_new_scene_dir(scene_dir, authoring_path, runtime_path, created_scene_dir);
        return false;
    }
    if (!LineDrawingCanonicalScene_ExportLayoutToFile(layout, scene_id, authoring_path)) {
        write_diagnostics(diagnostics, diagnostics_size, "failed to export canonical authoring scene");
        cleanup_failed_new_scene_dir(scene_dir, authoring_path, runtime_path, created_scene_dir);
        return false;
    }
    if (getenv("LINE_DRAWING_TEST_FORCE_SCENE_EXPORT_COMPILE_FAIL")) {
        write_diagnostics(diagnostics, diagnostics_size, "forced scene compile failure");
        cleanup_failed_new_scene_dir(scene_dir, authoring_path, runtime_path, created_scene_dir);
        return false;
    }

    compile_result = core_scene_compile_authoring_file_to_runtime_file(authoring_path,
                                                                       runtime_path,
                                                                       diagnostics,
                                                                       diagnostics_size);
    if (compile_result.code != CORE_OK) {
        if (!diagnostics || diagnostics[0] == '\0') {
            write_diagnostics(diagnostics, diagnostics_size, "scene compile failed");
        }
        cleanup_failed_new_scene_dir(scene_dir, authoring_path, runtime_path, created_scene_dir);
        return false;
    }

    if (out_paths) {
        snprintf(out_paths->scene_id, sizeof(out_paths->scene_id), "%s", scene_id);
        snprintf(out_paths->scene_dir, sizeof(out_paths->scene_dir), "%s", scene_dir);
        snprintf(out_paths->authoring_path, sizeof(out_paths->authoring_path), "%s", authoring_path);
        snprintf(out_paths->runtime_path, sizeof(out_paths->runtime_path), "%s", runtime_path);
    }
    return true;
}

bool LineDrawingSceneExport_ExportLayoutToAuthoringPath(const Layout* layout,
                                                        const char* authoring_path,
                                                        LineDrawingSceneExportPaths* out_paths,
                                                        char* diagnostics,
                                                        size_t diagnostics_size) {
    char scene_dir[SHAPE_EXPORT_PATH_MAX];
    char runtime_path[SHAPE_EXPORT_PATH_MAX];
    char scene_id[64];
    const char* parent_stem = NULL;
    CoreResult compile_result = {0};

    write_diagnostics(diagnostics, diagnostics_size, NULL);
    if (!layout) {
        write_diagnostics(diagnostics, diagnostics_size, "layout missing");
        return false;
    }
    if (!authoring_path || !authoring_path[0]) {
        write_diagnostics(diagnostics, diagnostics_size, "authoring path missing");
        return false;
    }
    if (!extract_directory_path(authoring_path, scene_dir, sizeof(scene_dir))) {
        write_diagnostics(diagnostics, diagnostics_size, "failed to resolve scene directory");
        return false;
    }
    if (!ensure_directory_exists(scene_dir)) {
        write_diagnostics(diagnostics, diagnostics_size, "failed to create scene directory");
        return false;
    }
    if (!ShapeExport_BuildPathInRoot(scene_dir, k_runtime_filename, runtime_path, sizeof(runtime_path))) {
        write_diagnostics(diagnostics, diagnostics_size, "failed to prepare runtime export path");
        return false;
    }

    parent_stem = extract_parent_basename(authoring_path);
    build_scene_id_from_stem(parent_stem ? parent_stem : k_default_scene_stem, scene_id, sizeof(scene_id));
    if (!LineDrawingCanonicalScene_ExportLayoutToFile(layout, scene_id, authoring_path)) {
        write_diagnostics(diagnostics, diagnostics_size, "failed to export canonical authoring scene");
        return false;
    }

    compile_result = core_scene_compile_authoring_file_to_runtime_file(authoring_path,
                                                                       runtime_path,
                                                                       diagnostics,
                                                                       diagnostics_size);
    if (compile_result.code != CORE_OK) {
        if (!diagnostics || diagnostics[0] == '\0') {
            write_diagnostics(diagnostics, diagnostics_size, "scene compile failed");
        }
        return false;
    }

    if (out_paths) {
        snprintf(out_paths->scene_id, sizeof(out_paths->scene_id), "%s", scene_id);
        snprintf(out_paths->scene_dir, sizeof(out_paths->scene_dir), "%s", scene_dir);
        snprintf(out_paths->authoring_path, sizeof(out_paths->authoring_path), "%s", authoring_path);
        snprintf(out_paths->runtime_path, sizeof(out_paths->runtime_path), "%s", runtime_path);
    }
    return true;
}

bool LineDrawingSceneExport_ExportLayoutToProjectRoot(const Layout* layout,
                                                      const char* project_root,
                                                      LineDrawingSceneExportPaths* out_paths,
                                                      char* diagnostics,
                                                      size_t diagnostics_size) {
    char authoring_path[SHAPE_EXPORT_PATH_MAX];
    LineDrawingSceneExportPaths export_paths;
    LineDrawingSceneProjectExportOptions project_options = {0};
    LineDrawingSceneProjectManifestObject* manifest_objects = NULL;
    size_t manifest_object_count = 0u;
    const char* project_name = NULL;
    bool ok = false;

    write_diagnostics(diagnostics, diagnostics_size, NULL);
    if (!layout) {
        write_diagnostics(diagnostics, diagnostics_size, "layout missing");
        return false;
    }
    if (!project_root || !project_root[0]) {
        write_diagnostics(diagnostics, diagnostics_size, "project root missing");
        return false;
    }
    if (!ShapeExport_BuildPathInRoot(project_root,
                                     k_authoring_filename,
                                     authoring_path,
                                     sizeof(authoring_path))) {
        write_diagnostics(diagnostics, diagnostics_size, "failed to prepare project authoring path");
        return false;
    }
    if (!LineDrawingSceneExport_ExportLayoutToAuthoringPath(layout,
                                                           authoring_path,
                                                           &export_paths,
                                                           diagnostics,
                                                           diagnostics_size)) {
        return false;
    }

    project_name = extract_basename(project_root);
    project_options.project_name = (project_name && project_name[0]) ? project_name : export_paths.scene_id;
    project_options.created_by = "line_drawing";
    project_options.authoring_scene = k_authoring_filename;
    project_options.runtime_scene = k_runtime_filename;
    manifest_object_count = count_project_manifest_mesh_objects(layout);
    if (manifest_object_count > 0u) {
        manifest_objects = (LineDrawingSceneProjectManifestObject*)calloc(
            manifest_object_count,
            sizeof(LineDrawingSceneProjectManifestObject));
        if (!manifest_objects) {
            write_diagnostics(diagnostics, diagnostics_size, "failed to allocate project object manifest");
            return false;
        }
        if (!fill_project_manifest_mesh_objects(layout, manifest_objects, manifest_object_count)) {
            free(manifest_objects);
            write_diagnostics(diagnostics, diagnostics_size, "failed to collect project object manifest");
            return false;
        }
        project_options.objects = manifest_objects;
        project_options.object_count = manifest_object_count;
    }
    if (!LineDrawingSceneProjectExport_WriteProjectFiles(project_root,
                                                        &project_options,
                                                        diagnostics,
                                                        diagnostics_size)) {
        free(manifest_objects);
        return false;
    }

    if (out_paths) {
        *out_paths = export_paths;
    }
    ok = true;
    free(manifest_objects);
    return ok;
}
