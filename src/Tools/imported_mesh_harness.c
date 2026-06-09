#include "core_io.h"
#include "core_mesh_asset.h"
#include "core_mesh_compile.h"
#include "core_units.h"
#include "cjson/cJSON.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define IMPORTED_MESH_HARNESS_PATH_MAX 1024
#ifndef PATH_MAX
#define PATH_MAX IMPORTED_MESH_HARNESS_PATH_MAX
#endif

typedef struct ImportedMeshHarnessOptions {
    const char* stl_path;
    const char* out_dir;
    const char* asset_id;
    const char* scene_id;
    const char* object_id;
} ImportedMeshHarnessOptions;

typedef struct ImportedMeshHarnessPaths {
    char source_path[IMPORTED_MESH_HARNESS_PATH_MAX];
    char authoring_dir[IMPORTED_MESH_HARNESS_PATH_MAX];
    char asset_dir[IMPORTED_MESH_HARNESS_PATH_MAX];
    char authoring_path[IMPORTED_MESH_HARNESS_PATH_MAX];
    char runtime_path[IMPORTED_MESH_HARNESS_PATH_MAX];
    char scene_path[IMPORTED_MESH_HARNESS_PATH_MAX];
    char summary_path[IMPORTED_MESH_HARNESS_PATH_MAX];
} ImportedMeshHarnessPaths;

static void print_usage(const char* argv0) {
    fprintf(stderr,
            "usage: %s --stl <model.stl> --out <dir> "
            "[--asset-id <asset_id>] [--scene-id <scene_id>] [--object-id <object_id>]\n",
            argv0 ? argv0 : "imported_mesh_harness");
}

static bool parse_args(int argc, char** argv, ImportedMeshHarnessOptions* out) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    out->asset_id = "asset_imported_mesh_01";
    out->scene_id = "scene_line_drawing_imported_mesh_harness";
    out->object_id = "obj_imported_mesh_01";
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--stl") == 0) {
            if (++i >= argc || !argv[i][0]) return false;
            out->stl_path = argv[i];
        } else if (strcmp(argv[i], "--out") == 0) {
            if (++i >= argc || !argv[i][0]) return false;
            out->out_dir = argv[i];
        } else if (strcmp(argv[i], "--asset-id") == 0) {
            if (++i >= argc || !argv[i][0]) return false;
            out->asset_id = argv[i];
        } else if (strcmp(argv[i], "--scene-id") == 0) {
            if (++i >= argc || !argv[i][0]) return false;
            out->scene_id = argv[i];
        } else if (strcmp(argv[i], "--object-id") == 0) {
            if (++i >= argc || !argv[i][0]) return false;
            out->object_id = argv[i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else {
            return false;
        }
    }
    return out->stl_path && out->out_dir;
}

static bool copy_text(char* dst, size_t dst_size, const char* src) {
    size_t len = 0u;
    if (!dst || dst_size == 0u || !src || src[0] == '\0') return false;
    len = strlen(src);
    if (len >= dst_size) return false;
    memcpy(dst, src, len + 1u);
    return true;
}

static bool join_path(char* out, size_t out_size, const char* dir, const char* name) {
    if (!out || !dir || !name || !dir[0] || !name[0]) return false;
    return snprintf(out, out_size, "%s/%s", dir, name) < (int)out_size;
}

static bool ensure_dir(const char* path) {
    char tmp[IMPORTED_MESH_HARNESS_PATH_MAX];
    size_t len = 0u;
    if (!path || !path[0]) return false;
    if (snprintf(tmp, sizeof(tmp), "%s", path) >= (int)sizeof(tmp)) return false;
    len = strlen(tmp);
    while (len > 1u && tmp[len - 1u] == '/') {
        tmp[len - 1u] = '\0';
        --len;
    }
    for (char* p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0775) != 0 && errno != EEXIST) return false;
            *p = '/';
        }
    }
    return mkdir(tmp, 0775) == 0 || errno == EEXIST;
}

static bool resolve_source_path(const char* path, char* out, size_t out_size) {
    char resolved[PATH_MAX];
    if (!path || !out || out_size == 0u) return false;
    if (!realpath(path, resolved)) return false;
    return copy_text(out, out_size, resolved);
}

static bool build_paths(const ImportedMeshHarnessOptions* options,
                        ImportedMeshHarnessPaths* out) {
    char filename[256];
    if (!options || !out) return false;
    memset(out, 0, sizeof(*out));
    if (!resolve_source_path(options->stl_path, out->source_path, sizeof(out->source_path))) {
        return false;
    }
    if (!join_path(out->authoring_dir, sizeof(out->authoring_dir), options->out_dir, "authoring") ||
        !join_path(out->asset_dir, sizeof(out->asset_dir), options->out_dir, "assets/mesh_assets")) {
        return false;
    }
    if (snprintf(filename, sizeof(filename), "%s.authoring.json", options->asset_id) >=
        (int)sizeof(filename)) {
        return false;
    }
    if (!join_path(out->authoring_path, sizeof(out->authoring_path), out->authoring_dir, filename)) {
        return false;
    }
    if (snprintf(filename, sizeof(filename), "%s.runtime.json", options->asset_id) >=
        (int)sizeof(filename)) {
        return false;
    }
    if (!join_path(out->runtime_path, sizeof(out->runtime_path), out->asset_dir, filename) ||
        !join_path(out->scene_path, sizeof(out->scene_path), options->out_dir, "scene_runtime.json") ||
        !join_path(out->summary_path, sizeof(out->summary_path), options->out_dir, "import_summary.json")) {
        return false;
    }
    return true;
}

static void add_vec3(cJSON* object, const char* key, double x, double y, double z) {
    cJSON* value = cJSON_CreateObject();
    if (!object || !key || !value) return;
    cJSON_AddNumberToObject(value, "x", x);
    cJSON_AddNumberToObject(value, "y", y);
    cJSON_AddNumberToObject(value, "z", z);
    cJSON_AddItemToObject(object, key, value);
}

static bool write_json_file(const char* path, cJSON* root) {
    char* text = NULL;
    CoreResult result;
    if (!path || !root) return false;
    text = cJSON_Print(root);
    if (!text) return false;
    result = core_io_write_all(path, text, strlen(text));
    cJSON_free(text);
    return result.code == CORE_OK;
}

static bool write_scene_runtime(const ImportedMeshHarnessOptions* options,
                                const ImportedMeshHarnessPaths* paths) {
    cJSON* root = cJSON_CreateObject();
    cJSON* objects = cJSON_CreateArray();
    cJSON* object = cJSON_CreateObject();
    cJSON* transform = cJSON_CreateObject();
    cJSON* geometry_ref = cJSON_CreateObject();
    cJSON* material_ref = cJSON_CreateObject();
    cJSON* flags = cJSON_CreateObject();
    cJSON* materials = cJSON_CreateArray();
    cJSON* material = cJSON_CreateObject();
    cJSON* base_color = cJSON_CreateObject();
    bool ok = false;

    if (!root || !objects || !object || !transform || !geometry_ref ||
        !material_ref || !flags || !materials || !material || !base_color) {
        goto done;
    }

    cJSON_AddStringToObject(root, "schema_family", "codework_scene");
    cJSON_AddStringToObject(root, "schema_variant", "scene_runtime_v1");
    cJSON_AddNumberToObject(root, "schema_version", 1);
    cJSON_AddStringToObject(root, "scene_id", options->scene_id);
    cJSON_AddStringToObject(root, "source_scene_id", options->scene_id);
    cJSON_AddStringToObject(root, "space_mode_default", "3d");
    cJSON_AddStringToObject(root, "unit_system", "meters");
    cJSON_AddNumberToObject(root, "world_scale", 1.0);

    cJSON_AddStringToObject(object, "object_id", options->object_id);
    cJSON_AddStringToObject(object, "object_type", "mesh_asset_instance");
    cJSON_AddStringToObject(object, "dimensional_mode", "full_3d");
    cJSON_AddStringToObject(object, "locked_plane", "xy");
    add_vec3(transform, "position", 0.0, 0.0, 0.0);
    add_vec3(transform, "rotation", 0.0, 0.0, 0.0);
    add_vec3(transform, "scale", 1.0, 1.0, 1.0);
    cJSON_AddItemToObject(object, "transform", transform);
    transform = NULL;

    cJSON_AddStringToObject(geometry_ref, "kind", "mesh_asset");
    cJSON_AddStringToObject(geometry_ref, "id", options->asset_id);
    cJSON_AddItemToObject(object, "geometry_ref", geometry_ref);
    geometry_ref = NULL;
    cJSON_AddStringToObject(material_ref, "id", "mat_imported_surface");
    cJSON_AddItemToObject(object, "material_ref", material_ref);
    material_ref = NULL;
    cJSON_AddBoolToObject(flags, "visible", true);
    cJSON_AddBoolToObject(flags, "locked", false);
    cJSON_AddBoolToObject(flags, "selectable", true);
    cJSON_AddItemToObject(object, "flags", flags);
    flags = NULL;
    cJSON_AddItemToArray(objects, object);
    object = NULL;
    cJSON_AddItemToObject(root, "objects", objects);
    objects = NULL;

    cJSON_AddStringToObject(material, "id", "mat_imported_surface");
    cJSON_AddStringToObject(material, "name", "Imported surface");
    cJSON_AddNumberToObject(base_color, "r", 0.8);
    cJSON_AddNumberToObject(base_color, "g", 0.7);
    cJSON_AddNumberToObject(base_color, "b", 0.55);
    cJSON_AddItemToObject(material, "base_color", base_color);
    base_color = NULL;
    cJSON_AddNumberToObject(material, "roughness", 0.6);
    cJSON_AddNumberToObject(material, "metallic", 0.0);
    cJSON_AddItemToArray(materials, material);
    material = NULL;
    cJSON_AddItemToObject(root, "materials", materials);
    materials = NULL;
    cJSON_AddItemToObject(root, "lights", cJSON_CreateArray());
    cJSON_AddItemToObject(root, "extensions", cJSON_CreateObject());

    ok = write_json_file(paths->scene_path, root);

done:
    cJSON_Delete(base_color);
    cJSON_Delete(material);
    cJSON_Delete(materials);
    cJSON_Delete(flags);
    cJSON_Delete(material_ref);
    cJSON_Delete(geometry_ref);
    cJSON_Delete(transform);
    cJSON_Delete(object);
    cJSON_Delete(objects);
    cJSON_Delete(root);
    return ok;
}

static bool write_summary(const ImportedMeshHarnessOptions* options,
                          const ImportedMeshHarnessPaths* paths,
                          const CoreMeshAssetRuntimeDocument* runtime) {
    cJSON* root = cJSON_CreateObject();
    bool ok = false;
    if (!root || !options || !paths || !runtime) {
        cJSON_Delete(root);
        return false;
    }
    cJSON_AddStringToObject(root, "schema", "line_drawing_imported_mesh_harness_summary_v1");
    cJSON_AddStringToObject(root, "source_stl", paths->source_path);
    cJSON_AddStringToObject(root, "asset_id", options->asset_id);
    cJSON_AddStringToObject(root, "authoring", paths->authoring_path);
    cJSON_AddStringToObject(root, "runtime", paths->runtime_path);
    cJSON_AddStringToObject(root, "scene_runtime", paths->scene_path);
    cJSON_AddNumberToObject(root, "vertices", (double)runtime->vertex_count);
    cJSON_AddNumberToObject(root, "triangles", (double)runtime->triangle_count);
    cJSON_AddNumberToObject(root, "surface_groups", (double)runtime->surface_group_count);
    ok = write_json_file(paths->summary_path, root);
    cJSON_Delete(root);
    return ok;
}

static bool populate_authoring_document(const ImportedMeshHarnessOptions* options,
                                        const ImportedMeshHarnessPaths* paths,
                                        CoreMeshAssetAuthoringDocument* document) {
    if (!options || !paths || !document) return false;
    core_mesh_asset_authoring_document_init(document);
    if (core_mesh_asset_authoring_contract_set_asset_id(&document->contract,
                                                        options->asset_id).code != CORE_OK) {
        return false;
    }
    document->contract.unit_kind = CORE_UNIT_METER;
    document->contract.world_scale = 1.0;
    document->contract.asset_type = CORE_MESH_ASSET_TYPE_SOLID_MESH;
    document->contract.source_mode = CORE_MESH_ASSET_SOURCE_MODE_IMPORTED_MESH;
    document->contract.topology_closed_volume_expected = true;
    document->contract.topology_manifold_expected = true;

    document->has_imported_mesh_source = true;
    core_mesh_asset_imported_mesh_source_init(&document->imported_mesh_source);
    if (!copy_text(document->imported_mesh_source.import_id,
                   sizeof(document->imported_mesh_source.import_id),
                   "import_stl_01") ||
        !copy_text(document->imported_mesh_source.source_uri,
                   sizeof(document->imported_mesh_source.source_uri),
                   paths->source_path) ||
        !copy_text(document->imported_mesh_source.orientation_policy,
                   sizeof(document->imported_mesh_source.orientation_policy),
                   "source_axes") ||
        !copy_text(document->imported_mesh_source.default_surface_group_id,
                   sizeof(document->imported_mesh_source.default_surface_group_id),
                   "imported_surface")) {
        return false;
    }
    document->imported_mesh_source.source_format =
        CORE_MESH_ASSET_IMPORTED_MESH_SOURCE_FORMAT_STL;
    document->imported_mesh_source.source_unit_kind = CORE_UNIT_METER;
    document->imported_mesh_source.source_to_asset_scale = 1.0;
    document->imported_mesh_source.weld_vertices = true;
    document->imported_mesh_source.weld_tolerance = 0.000001;
    document->imported_mesh_source.preserve_source_normals = false;
    document->imported_mesh_source.topology_closed_volume_observed = true;
    document->imported_mesh_source.topology_manifold_observed = true;
    return core_mesh_asset_authoring_document_validate(document).code == CORE_OK;
}

int main(int argc, char** argv) {
    ImportedMeshHarnessOptions options;
    ImportedMeshHarnessPaths paths;
    CoreMeshAssetAuthoringDocument authoring;
    CoreMeshAssetAuthoringDocument reloaded_authoring;
    CoreMeshAssetRuntimeDocument runtime;
    CoreResult result;
    int exit_code = 1;

    core_mesh_asset_authoring_document_init(&authoring);
    core_mesh_asset_authoring_document_init(&reloaded_authoring);
    core_mesh_asset_runtime_document_init(&runtime);

    if (!parse_args(argc, argv, &options)) {
        print_usage(argv[0]);
        goto done;
    }
    if (!build_paths(&options, &paths)) {
        fprintf(stderr, "imported_mesh_harness: failed to prepare output paths\n");
        goto done;
    }
    if (!ensure_dir(options.out_dir) || !ensure_dir(paths.authoring_dir) ||
        !ensure_dir(paths.asset_dir)) {
        fprintf(stderr, "imported_mesh_harness: failed to create output directories\n");
        goto done;
    }
    if (!populate_authoring_document(&options, &paths, &authoring)) {
        fprintf(stderr, "imported_mesh_harness: failed to create authoring metadata\n");
        goto done;
    }

    result = core_mesh_asset_authoring_document_save_file(&authoring, paths.authoring_path);
    if (result.code != CORE_OK) {
        fprintf(stderr, "imported_mesh_harness: authoring save failed: %s\n", result.message);
        goto done;
    }
    result = core_mesh_asset_authoring_document_load_file(paths.authoring_path, &reloaded_authoring);
    if (result.code != CORE_OK) {
        fprintf(stderr, "imported_mesh_harness: authoring reload failed: %s\n", result.message);
        goto done;
    }
    result = core_mesh_compile_imported_mesh_to_runtime_file(&reloaded_authoring,
                                                             NULL,
                                                             options.asset_id,
                                                             paths.runtime_path);
    if (result.code != CORE_OK) {
        fprintf(stderr, "imported_mesh_harness: runtime compile failed: %s\n", result.message);
        goto done;
    }
    result = core_mesh_asset_runtime_document_load_file(paths.runtime_path, &runtime);
    if (result.code != CORE_OK) {
        fprintf(stderr, "imported_mesh_harness: runtime reload failed: %s\n", result.message);
        goto done;
    }
    if (!write_scene_runtime(&options, &paths) || !write_summary(&options, &paths, &runtime)) {
        fprintf(stderr, "imported_mesh_harness: failed to write scene or summary\n");
        goto done;
    }

    printf("authoring=%s\nruntime=%s\nscene_runtime=%s\nsummary=%s\n",
           paths.authoring_path,
           paths.runtime_path,
           paths.scene_path,
           paths.summary_path);
    exit_code = 0;

done:
    core_mesh_asset_runtime_document_free(&runtime);
    core_mesh_asset_authoring_document_free(&reloaded_authoring);
    core_mesh_asset_authoring_document_free(&authoring);
    return exit_code;
}
