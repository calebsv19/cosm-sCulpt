#include "Tools/scene_project_export.h"

#include "Tools/shape_export.h"
#include "cjson/cJSON.h"

#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char* k_default_project_name = "line_drawing_scene_project";
static const char* k_default_created_by = "line_drawing";
static const char* k_default_timestamp = "1970-01-01T00:00:00Z";
static const char* k_default_authoring_scene = "scene_authoring.json";
static const char* k_default_runtime_scene = "scene_runtime.json";

static void write_diagnostics(char* diagnostics, size_t diagnostics_size, const char* message) {
    if (!diagnostics || diagnostics_size == 0u) return;
    if (!message) {
        diagnostics[0] = '\0';
        return;
    }
    snprintf(diagnostics, diagnostics_size, "%s", message);
}

static bool ensure_directory_exists(const char* path) {
    struct stat st;
    if (!path || !path[0]) return false;
    if (stat(path, &st) == 0) return S_ISDIR(st.st_mode);
    if (mkdir(path, 0755) == 0) return true;
    return errno == EEXIST;
}

static bool build_path(const char* root, const char* leaf, char* out_path, size_t out_path_size) {
    const size_t root_len = root ? strlen(root) : 0u;
    const bool needs_slash = root_len > 0u && root[root_len - 1u] != '/';
    int written = 0;
    if (!root || !root[0] || !leaf || !leaf[0] || !out_path || out_path_size == 0u) return false;
    written = snprintf(out_path, out_path_size, "%s%s%s", root, needs_slash ? "/" : "", leaf);
    return written > 0 && (size_t)written < out_path_size;
}

static bool ensure_relative_directory(const char* project_root, const char* relative_path) {
    char path[SHAPE_EXPORT_PATH_MAX];
    if (!build_path(project_root, relative_path, path, sizeof(path))) return false;
    return ensure_directory_exists(path);
}

static bool write_text_file(const char* path, const char* text) {
    FILE* fp = NULL;
    const size_t len = text ? strlen(text) : 0u;
    if (!path || !path[0] || !text) return false;
    fp = fopen(path, "wb");
    if (!fp) return false;
    if (len > 0u && fwrite(text, 1u, len, fp) != len) {
        fclose(fp);
        return false;
    }
    return fclose(fp) == 0;
}

static bool copy_file(const char* source_path, const char* dest_path) {
    FILE* in = NULL;
    FILE* out = NULL;
    unsigned char buffer[8192];
    size_t count = 0u;
    bool ok = false;
    if (!source_path || !source_path[0] || !dest_path || !dest_path[0]) return false;
    in = fopen(source_path, "rb");
    if (!in) return false;
    out = fopen(dest_path, "wb");
    if (!out) {
        fclose(in);
        return false;
    }
    while ((count = fread(buffer, 1u, sizeof(buffer), in)) > 0u) {
        if (fwrite(buffer, 1u, count, out) != count) goto done;
    }
    if (ferror(in)) goto done;
    ok = true;

done:
    if (fclose(out) != 0) ok = false;
    fclose(in);
    return ok;
}

static bool write_json_file(const char* path, cJSON* root) {
    char* text = NULL;
    bool ok = false;
    if (!path || !path[0] || !root) return false;
    text = cJSON_Print(root);
    if (!text) return false;
    ok = write_text_file(path, text);
    cJSON_free(text);
    return ok;
}

static const char* string_or_default(const char* value, const char* fallback) {
    return (value && value[0]) ? value : fallback;
}

static bool build_mesh_sidecar_filename(const LineDrawingSceneProjectManifestObject* object,
                                        char* out_filename,
                                        size_t out_filename_size) {
    const char* seed = NULL;
    size_t out = 0u;
    if (!object || !out_filename || out_filename_size == 0u) return false;
    seed = string_or_default(object->mesh_asset_id, object->object_id);
    if (!seed || !seed[0]) seed = "mesh_asset";
    for (const char* p = seed; *p && out + 1u < out_filename_size; ++p) {
        const unsigned char ch = (unsigned char)*p;
        if (isalnum(ch) || ch == '_' || ch == '-' || ch == '.') {
            out_filename[out++] = (char)ch;
        } else if (out > 0u && out_filename[out - 1u] != '_') {
            out_filename[out++] = '_';
        }
    }
    while (out > 0u && out_filename[out - 1u] == '_') --out;
    if (out == 0u) {
        const char* fallback = "mesh_asset";
        const size_t fallback_len = strlen(fallback);
        if (fallback_len + strlen(".runtime.json") >= out_filename_size) return false;
        memcpy(out_filename, fallback, fallback_len);
        out = fallback_len;
    }
    if (out + strlen(".runtime.json") >= out_filename_size) return false;
    memcpy(out_filename + out, ".runtime.json", strlen(".runtime.json") + 1u);
    return true;
}

static bool build_mesh_sidecar_relative_path(const char* filename,
                                             char* out_relative_path,
                                             size_t out_relative_path_size) {
    int written = 0;
    if (!filename || !filename[0] || !out_relative_path || out_relative_path_size == 0u) return false;
    written = snprintf(out_relative_path, out_relative_path_size, "assets/mesh_assets/%s", filename);
    return written > 0 && (size_t)written < out_relative_path_size;
}

static bool copy_manifest_mesh_sidecars(const char* project_root,
                                        const LineDrawingSceneProjectExportOptions* options) {
    if (!options || !options->objects || options->object_count == 0u) return true;
    for (size_t i = 0u; i < options->object_count; ++i) {
        const LineDrawingSceneProjectManifestObject* object = &options->objects[i];
        char filename[256];
        char relative_path[SHAPE_EXPORT_PATH_MAX];
        char dest_path[SHAPE_EXPORT_PATH_MAX];
        if (!object->source_mesh_sidecar_path || !object->source_mesh_sidecar_path[0]) continue;
        if (!build_mesh_sidecar_filename(object, filename, sizeof(filename))) return false;
        if (!build_mesh_sidecar_relative_path(filename, relative_path, sizeof(relative_path))) return false;
        if (!build_path(project_root, relative_path, dest_path, sizeof(dest_path))) return false;
        if (!copy_file(object->source_mesh_sidecar_path, dest_path)) return false;
    }
    return true;
}

static bool write_scene_project_json(const char* path,
                                     const LineDrawingSceneProjectExportOptions* options) {
    const char* project_name = options && options->project_name ? options->project_name : k_default_project_name;
    const char* created_by = options && options->created_by ? options->created_by : k_default_created_by;
    const char* timestamp = options && options->timestamp_utc ? options->timestamp_utc : k_default_timestamp;
    const char* authoring_scene =
        options && options->authoring_scene ? options->authoring_scene : k_default_authoring_scene;
    const char* runtime_scene =
        options && options->runtime_scene ? options->runtime_scene : k_default_runtime_scene;
    cJSON* root = cJSON_CreateObject();
    bool ok = false;
    if (!root) return false;

    cJSON_AddStringToObject(root, "schema", "codework_scene_project_v1");
    cJSON_AddStringToObject(root, "project_name", project_name);
    cJSON_AddStringToObject(root, "created_by", created_by);
    cJSON_AddStringToObject(root, "created_at", timestamp);
    cJSON_AddStringToObject(root, "updated_at", timestamp);
    cJSON_AddStringToObject(root, "authoring_scene", authoring_scene);
    cJSON_AddStringToObject(root, "runtime_scene", runtime_scene);
    cJSON_AddStringToObject(root, "object_manifest", "object_manifest.json");
    cJSON_AddStringToObject(root, "mesh_assets_dir", "assets/mesh_assets");
    cJSON_AddStringToObject(root, "active_cache", "physics_sim/active_cache_manifest.json");
    cJSON_AddStringToObject(root, "active_render_request", "ray_tracing/render_request.json");

    ok = write_json_file(path, root);
    cJSON_Delete(root);
    return ok;
}

static bool add_manifest_object(cJSON* objects, const LineDrawingSceneProjectManifestObject* object) {
    cJSON* item = NULL;
    char filename[256];
    char relative_path[SHAPE_EXPORT_PATH_MAX];
    const char* object_id = NULL;
    const char* kind = NULL;
    if (!objects || !object) return false;
    item = cJSON_CreateObject();
    if (!item) return false;
    object_id = string_or_default(object->object_id, object->mesh_asset_id);
    kind = string_or_default(object->kind, "mesh_asset_instance");
    cJSON_AddStringToObject(item, "id", string_or_default(object_id, "object"));
    cJSON_AddStringToObject(item, "name", string_or_default(object->display_name, string_or_default(object_id, "object")));
    cJSON_AddStringToObject(item, "kind", kind);
    if (object->mesh_asset_id && object->mesh_asset_id[0]) {
        cJSON_AddStringToObject(item, "mesh_asset_id", object->mesh_asset_id);
    }
    if (object->source_asset_id && object->source_asset_id[0]) {
        cJSON_AddStringToObject(item, "source_asset_id", object->source_asset_id);
    }
    if (object->source_mesh_sidecar_path && object->source_mesh_sidecar_path[0]) {
        if (!build_mesh_sidecar_filename(object, filename, sizeof(filename)) ||
            !build_mesh_sidecar_relative_path(filename, relative_path, sizeof(relative_path))) {
            cJSON_Delete(item);
            return false;
        }
        cJSON_AddStringToObject(item, "mesh_sidecar_path", relative_path);
    }
    cJSON_AddNumberToObject(item, "vertex_count", (double)object->vertex_count);
    cJSON_AddNumberToObject(item, "triangle_count", (double)object->triangle_count);
    cJSON_AddBoolToObject(item, "physics_extension_present", object->has_physics_extension);
    cJSON_AddBoolToObject(item, "ray_tracing_extension_present", object->has_ray_tracing_extension);
    cJSON_AddItemToArray(objects, item);
    return true;
}

static bool write_object_manifest_json(const char* path,
                                       const LineDrawingSceneProjectExportOptions* options) {
    cJSON* root = cJSON_CreateObject();
    cJSON* objects = NULL;
    bool ok = false;
    if (!root) return false;
    objects = cJSON_CreateArray();
    if (!objects) {
        cJSON_Delete(root);
        return false;
    }
    cJSON_AddStringToObject(root, "schema", "line_drawing_object_manifest_v1");
    cJSON_AddItemToObject(root, "objects", objects);
    if (options && options->objects && options->object_count > 0u) {
        for (size_t i = 0u; i < options->object_count; ++i) {
            if (!add_manifest_object(objects, &options->objects[i])) {
                cJSON_Delete(root);
                return false;
            }
        }
    }

    ok = write_json_file(path, root);
    cJSON_Delete(root);
    return ok;
}

bool LineDrawingSceneProjectExport_WriteProjectFiles(const char* project_root,
                                                     const LineDrawingSceneProjectExportOptions* options,
                                                     char* diagnostics,
                                                     size_t diagnostics_size) {
    static const char* k_directories[] = {
        "assets",
        "assets/mesh_assets",
        "assets/vf3d",
        "assets/vf3d/active",
        "assets/vf3d/runs",
        "assets/physics",
        "assets/physics/active",
        "assets/physics/runs",
        "line_drawing",
        "physics_sim",
        "physics_sim/runs",
        "ray_tracing",
        "ray_tracing/presets",
        "ray_tracing/frames_temp",
        "ray_tracing/videos",
        "ray_tracing/runs",
        "ray_tracing/review",
        "worker_export",
    };
    char scene_project_path[SHAPE_EXPORT_PATH_MAX];
    char object_manifest_path[SHAPE_EXPORT_PATH_MAX];
    char notes_path[SHAPE_EXPORT_PATH_MAX];

    write_diagnostics(diagnostics, diagnostics_size, NULL);
    if (!project_root || !project_root[0]) {
        write_diagnostics(diagnostics, diagnostics_size, "project root missing");
        return false;
    }
    if (!ensure_directory_exists(project_root)) {
        write_diagnostics(diagnostics, diagnostics_size, "failed to create project root");
        return false;
    }
    for (size_t i = 0u; i < sizeof(k_directories) / sizeof(k_directories[0]); ++i) {
        if (!ensure_relative_directory(project_root, k_directories[i])) {
            write_diagnostics(diagnostics, diagnostics_size, "failed to create project scaffold directory");
            return false;
        }
    }
    if (!build_path(project_root, "scene_project.json", scene_project_path, sizeof(scene_project_path)) ||
        !build_path(project_root, "object_manifest.json", object_manifest_path, sizeof(object_manifest_path)) ||
        !build_path(project_root, "line_drawing/notes.md", notes_path, sizeof(notes_path))) {
        write_diagnostics(diagnostics, diagnostics_size, "failed to build project file path");
        return false;
    }
    if (!write_scene_project_json(scene_project_path, options)) {
        write_diagnostics(diagnostics, diagnostics_size, "failed to write scene_project.json");
        return false;
    }
    if (!copy_manifest_mesh_sidecars(project_root, options)) {
        write_diagnostics(diagnostics, diagnostics_size, "failed to copy project mesh sidecar");
        return false;
    }
    if (!write_object_manifest_json(object_manifest_path, options)) {
        write_diagnostics(diagnostics, diagnostics_size, "failed to write object_manifest.json");
        return false;
    }
    if (!write_text_file(notes_path, "")) {
        write_diagnostics(diagnostics, diagnostics_size, "failed to write line_drawing/notes.md");
        return false;
    }
    return true;
}
