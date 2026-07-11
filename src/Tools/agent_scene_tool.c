#include "Layout/layout.h"
#include "Tools/agent_scene_material_flow.h"
#include "Layout/layout_json.h"
#include "Tools/canonical_scene_export.h"
#include "core_scene_compile.h"
#include "cjson/cJSON.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define AGENT_SCENE_PATH_MAX 1024

typedef struct AgentSceneOptions {
    const char* request_path;
    const char* output_dir;
    bool determinism_check;
} AgentSceneOptions;

typedef struct AgentSceneCounts {
    int planes;
    int rect_prisms;
    int mesh_asset_instances;
    int mesh_assets_copied;
    int bounds_adjusted;
} AgentSceneCounts;

static void print_usage(const char* argv0) {
    fprintf(stderr,
            "usage: %s --request <agent_scene_request.json> --out <scene_dir> [--determinism-check]\n",
            argv0 ? argv0 : "agent_scene_tool");
}

static bool parse_args(int argc, char** argv, AgentSceneOptions* out) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--request") == 0) {
            if (++i >= argc || !argv[i][0]) return false;
            out->request_path = argv[i];
        } else if (strcmp(argv[i], "--out") == 0) {
            if (++i >= argc || !argv[i][0]) return false;
            out->output_dir = argv[i];
        } else if (strcmp(argv[i], "--determinism-check") == 0) {
            out->determinism_check = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else {
            return false;
        }
    }
    return out->request_path && out->output_dir;
}

static bool read_text_file(const char* path, char** out_text) {
    FILE* f = NULL;
    long len = 0;
    char* data = NULL;
    if (!path || !out_text) return false;
    *out_text = NULL;

    f = fopen(path, "rb");
    if (!f) return false;
    if (fseek(f, 0, SEEK_END) != 0) goto fail;
    len = ftell(f);
    if (len < 0) goto fail;
    if (fseek(f, 0, SEEK_SET) != 0) goto fail;

    data = (char*)malloc((size_t)len + 1u);
    if (!data) goto fail;
    if (fread(data, 1u, (size_t)len, f) != (size_t)len) goto fail;
    data[len] = '\0';
    fclose(f);
    *out_text = data;
    return true;

fail:
    free(data);
    fclose(f);
    return false;
}

static bool write_text_file(const char* path, const char* text) {
    FILE* f = NULL;
    if (!path || !text) return false;
    f = fopen(path, "wb");
    if (!f) return false;
    if (fwrite(text, 1u, strlen(text), f) != strlen(text)) {
        fclose(f);
        return false;
    }
    return fclose(f) == 0;
}

static bool copy_text_file(const char* source_path, const char* dest_path) {
    char* text = NULL;
    bool ok = false;
    if (!read_text_file(source_path, &text)) return false;
    ok = write_text_file(dest_path, text);
    free(text);
    return ok;
}

static bool ensure_dir(const char* path) {
    char tmp[AGENT_SCENE_PATH_MAX];
    size_t len = 0;
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

static bool join_path(char* out, size_t out_size, const char* dir, const char* name) {
    if (!out || !dir || !name) return false;
    return snprintf(out, out_size, "%s/%s", dir, name) < (int)out_size;
}

static bool path_is_absolute(const char* path) {
    return path && path[0] == '/';
}

static bool path_dirname(char* out, size_t out_size, const char* path) {
    const char* last_slash = NULL;
    size_t len = 0u;
    if (!out || out_size == 0u || !path || !path[0]) return false;
    last_slash = strrchr(path, '/');
    if (!last_slash || last_slash == path) return false;
    len = (size_t)(last_slash - path);
    if (len >= out_size) return false;
    memcpy(out, path, len);
    out[len] = '\0';
    return true;
}

static bool resolve_request_relative_path(char* out,
                                          size_t out_size,
                                          const char* request_path,
                                          const char* source_path) {
    char request_dir[AGENT_SCENE_PATH_MAX];
    if (!out || out_size == 0u || !source_path || !source_path[0]) return false;
    if (path_is_absolute(source_path)) {
        return snprintf(out, out_size, "%s", source_path) < (int)out_size;
    }
    if (!path_dirname(request_dir, sizeof(request_dir), request_path)) {
        snprintf(request_dir, sizeof(request_dir), ".");
    }
    return join_path(out, out_size, request_dir, source_path);
}

static const char* path_basename(const char* path) {
    const char* last_slash = NULL;
    if (!path || !path[0]) return "";
    last_slash = strrchr(path, '/');
    return last_slash ? last_slash + 1 : path;
}

static bool derive_app_load_paths(const char* output_dir,
                                  const char* scene_id,
                                  char* out_run_dir,
                                  size_t out_run_dir_size,
                                  char* out_layout_copy_path,
                                  size_t out_layout_copy_path_size,
                                  char* out_app_load_dir,
                                  size_t out_app_load_dir_size,
                                  char* out_app_authoring_path,
                                  size_t out_app_authoring_path_size,
                                  char* out_app_runtime_path,
                                  size_t out_app_runtime_path_size) {
    const char* output_base = path_basename(output_dir);
    const char* slug = NULL;
    char layout_name[256];
    if (!output_dir || !scene_id || !out_run_dir || !out_layout_copy_path || !out_app_load_dir ||
        !out_app_authoring_path || !out_app_runtime_path) {
        return false;
    }

    if (strcmp(output_base, "line_drawing") == 0) {
        if (!path_dirname(out_run_dir, out_run_dir_size, output_dir)) return false;
        slug = path_basename(out_run_dir);
    } else {
        if (snprintf(out_run_dir, out_run_dir_size, "%s", output_dir) >= (int)out_run_dir_size) return false;
        slug = scene_id;
    }
    if (!slug || !slug[0]) slug = scene_id;
    if (snprintf(layout_name, sizeof(layout_name), "%s.layout.json", slug) >= (int)sizeof(layout_name)) {
        return false;
    }
    if (!join_path(out_layout_copy_path, out_layout_copy_path_size, out_run_dir, layout_name)) return false;
    if (!join_path(out_app_load_dir, out_app_load_dir_size, out_run_dir, "line_drawing_app_load")) return false;
    if (!join_path(out_app_authoring_path,
                   out_app_authoring_path_size,
                   out_app_load_dir,
                   "scene_authoring.json")) {
        return false;
    }
    if (!join_path(out_app_runtime_path,
                   out_app_runtime_path_size,
                   out_app_load_dir,
                   "scene_runtime.json")) {
        return false;
    }
    return true;
}

static const char* json_string_or(const cJSON* object, const char* key, const char* fallback) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (cJSON_IsString(item) && item->valuestring && item->valuestring[0]) {
        return item->valuestring;
    }
    return fallback;
}

static bool json_bool_or(const cJSON* object, const char* key, bool fallback) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (cJSON_IsBool(item)) return cJSON_IsTrue(item);
    return fallback;
}

static double json_number_or(const cJSON* object, const char* key, double fallback) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (cJSON_IsNumber(item)) return item->valuedouble;
    return fallback;
}

static bool duplicate_into_object(cJSON* object, const char* key, const cJSON* value) {
    cJSON* duplicate = NULL;
    if (!object || !key || !key[0] || !value) return false;
    duplicate = cJSON_Duplicate(value, 1);
    if (!duplicate) return false;
    cJSON_DeleteItemFromObjectCaseSensitive(object, key);
    cJSON_AddItemToObject(object, key, duplicate);
    return true;
}

static bool merge_object_members(cJSON* target, const cJSON* source) {
    const cJSON* child = NULL;
    if (!target || !cJSON_IsObject(source)) return false;
    cJSON_ArrayForEach(child, source) {
        if (!child->string || !duplicate_into_object(target, child->string, child)) {
            return false;
        }
    }
    return true;
}

static cJSON* ensure_object_member(cJSON* object, const char* key) {
    cJSON* child = NULL;
    if (!object || !key || !key[0]) return NULL;
    child = cJSON_GetObjectItemCaseSensitive(object, key);
    if (cJSON_IsObject(child)) return child;
    if (child) cJSON_DeleteItemFromObjectCaseSensitive(object, key);
    child = cJSON_CreateObject();
    if (!child) return NULL;
    cJSON_AddItemToObject(object, key, child);
    return child;
}

static cJSON* ensure_array_member(cJSON* object, const char* key) {
    cJSON* child = NULL;
    if (!object || !key || !key[0]) return NULL;
    child = cJSON_GetObjectItemCaseSensitive(object, key);
    if (cJSON_IsArray(child)) return child;
    if (child) cJSON_DeleteItemFromObjectCaseSensitive(object, key);
    child = cJSON_CreateArray();
    if (!child) return NULL;
    cJSON_AddItemToObject(object, key, child);
    return child;
}

static bool parse_vec3_required(const cJSON* object, const char* key, Vec3* out) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(object, key);
    const cJSON* x = NULL;
    const cJSON* y = NULL;
    const cJSON* z = NULL;
    if (!cJSON_IsObject(item) || !out) return false;
    x = cJSON_GetObjectItemCaseSensitive(item, "x");
    y = cJSON_GetObjectItemCaseSensitive(item, "y");
    z = cJSON_GetObjectItemCaseSensitive(item, "z");
    if (!cJSON_IsNumber(x) || !cJSON_IsNumber(y) || !cJSON_IsNumber(z)) return false;
    *out = (Vec3){ (float)x->valuedouble, (float)y->valuedouble, (float)z->valuedouble };
    return true;
}

static bool parse_vec3_optional(const cJSON* object, const char* key, Vec3 fallback, Vec3* out) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(object, key);
    const cJSON* x = NULL;
    const cJSON* y = NULL;
    const cJSON* z = NULL;
    if (!out) return false;
    *out = fallback;
    if (!item) return true;
    if (!cJSON_IsObject(item)) return false;
    x = cJSON_GetObjectItemCaseSensitive(item, "x");
    y = cJSON_GetObjectItemCaseSensitive(item, "y");
    z = cJSON_GetObjectItemCaseSensitive(item, "z");
    if (!cJSON_IsNumber(x) || !cJSON_IsNumber(y) || !cJSON_IsNumber(z)) return false;
    *out = (Vec3){ (float)x->valuedouble, (float)y->valuedouble, (float)z->valuedouble };
    return true;
}

static bool add_vec3_json(cJSON* object, const char* key, Vec3 value) {
    cJSON* vec = NULL;
    if (!object || !key || !key[0]) return false;
    vec = cJSON_CreateObject();
    if (!vec) return false;
    cJSON_AddItemToObject(object, key, vec);
    cJSON_AddNumberToObject(vec, "x", value.x);
    cJSON_AddNumberToObject(vec, "y", value.y);
    cJSON_AddNumberToObject(vec, "z", value.z);
    return true;
}

static const char* mesh_asset_id_from_request(const cJSON* item) {
    const cJSON* geometry_ref = NULL;
    const char* asset_id = NULL;
    asset_id = json_string_or(item, "asset_id", NULL);
    if (asset_id && asset_id[0]) return asset_id;
    geometry_ref = cJSON_GetObjectItemCaseSensitive(item, "geometry_ref");
    if (cJSON_IsObject(geometry_ref)) {
        return json_string_or(geometry_ref, "id", NULL);
    }
    return NULL;
}

static bool material_id_exists(const cJSON* materials, const char* material_id) {
    const cJSON* material = NULL;
    if (!cJSON_IsArray(materials) || !material_id || !material_id[0]) return false;
    cJSON_ArrayForEach(material, materials) {
        if (strcmp(json_string_or(material, "material_id", ""), material_id) == 0) {
            return true;
        }
    }
    return false;
}

static bool ensure_mesh_material(cJSON* authoring, const cJSON* item) {
    const char* material_id = json_string_or(item, "material_id", NULL);
    cJSON* materials = NULL;
    cJSON* material = NULL;
    cJSON* extensions = NULL;
    cJSON* line_drawing = NULL;

    if (!authoring || !item || !material_id || !material_id[0]) return true;
    materials = ensure_array_member(authoring, "materials");
    if (!materials) return false;
    if (material_id_exists(materials, material_id)) return true;

    material = cJSON_CreateObject();
    extensions = cJSON_CreateObject();
    line_drawing = cJSON_CreateObject();
    if (!material || !extensions || !line_drawing) goto fail;

    cJSON_AddStringToObject(material, "material_id", material_id);
    cJSON_AddStringToObject(material, "material_type", json_string_or(item, "material_type", "flat_color"));
    cJSON_AddItemToObject(material, "extensions", extensions);
    extensions = NULL;
    cJSON_AddItemToObject(cJSON_GetObjectItemCaseSensitive(material, "extensions"), "line_drawing", line_drawing);
    line_drawing = NULL;
    cJSON_AddStringToObject(cJSON_GetObjectItemCaseSensitive(
                                cJSON_GetObjectItemCaseSensitive(material, "extensions"),
                                "line_drawing"),
                            "preset",
                            "mesh_asset");

    cJSON_AddItemToArray(materials, material);
    return true;

fail:
    cJSON_Delete(material);
    cJSON_Delete(extensions);
    cJSON_Delete(line_drawing);
    return false;
}

static bool apply_request_object_id(Object3D* object, const cJSON* item) {
    const char* request_id = json_string_or(item, "id", NULL);
    if (!object || !request_id) return true;
    return core_object_set_identity(&object->coreMeta, request_id, object->coreMeta.object_type).code == CORE_OK;
}

static bool parse_bounds(const cJSON* root, SceneBounds3D* out) {
    const cJSON* bounds = cJSON_GetObjectItemCaseSensitive(root, "bounds");
    if (!bounds) return true;
    if (!cJSON_IsObject(bounds) || !out) return false;
    out->enabled = json_bool_or(bounds, "enabled", true);
    out->clampOnEdit = json_bool_or(bounds, "clamp_on_edit", false);
    if (!parse_vec3_required(bounds, "min", &out->min)) return false;
    if (!parse_vec3_required(bounds, "max", &out->max)) return false;
    return Layout_SceneBounds3D_IsValid(out);
}

static bool parse_axis(const char* text, ViewPlaneAxis* out) {
    if (!text || !out) return false;
    if (strcmp(text, "xy") == 0) {
        *out = VIEW_PLANE_XY;
        return true;
    }
    if (strcmp(text, "yz") == 0) {
        *out = VIEW_PLANE_YZ;
        return true;
    }
    if (strcmp(text, "xz") == 0) {
        *out = VIEW_PLANE_XZ;
        return true;
    }
    return false;
}

static PlaneFrame3 frame_from_axis(ViewPlaneAxis axis, Vec3 origin) {
    switch (axis) {
        case VIEW_PLANE_YZ:
            return (PlaneFrame3){
                .origin = origin,
                .axisU = { 0.0f, 1.0f, 0.0f },
                .axisV = { 0.0f, 0.0f, 1.0f },
                .normal = { 1.0f, 0.0f, 0.0f }
            };
        case VIEW_PLANE_XZ:
            return (PlaneFrame3){
                .origin = origin,
                .axisU = { -1.0f, 0.0f, 0.0f },
                .axisV = { 0.0f, 0.0f, 1.0f },
                .normal = { 0.0f, 1.0f, 0.0f }
            };
        case VIEW_PLANE_XY:
        default:
            return (PlaneFrame3){
                .origin = origin,
                .axisU = { 1.0f, 0.0f, 0.0f },
                .axisV = { 0.0f, 1.0f, 0.0f },
                .normal = { 0.0f, 0.0f, 1.0f }
            };
    }
}

static CoreObjectPlane core_plane_from_axis(ViewPlaneAxis axis) {
    switch (axis) {
        case VIEW_PLANE_YZ: return CORE_OBJECT_PLANE_YZ;
        case VIEW_PLANE_XZ: return CORE_OBJECT_PLANE_XZ;
        case VIEW_PLANE_XY:
        default: return CORE_OBJECT_PLANE_XY;
    }
}

static bool apply_construction_plane(const cJSON* root, Layout* layout) {
    const cJSON* plane = cJSON_GetObjectItemCaseSensitive(root, "construction_plane");
    ViewPlaneAxis axis = VIEW_PLANE_XY;
    float offset = 0.0f;
    if (!plane) return true;
    if (!cJSON_IsObject(plane) || !layout) return false;
    if (!parse_axis(json_string_or(plane, "axis", "xy"), &axis)) return false;
    offset = (float)json_number_or(plane, "offset", 0.0);
    Layout_ConstructionPlane3D_SetFromViewPlane(
        &layout->scene3d.constructionPlane,
        (ViewPlane){ .axis = axis, .offset = offset });
    layout->scene3d.constructionPlane.customFrame =
        frame_from_axis(axis, Vec3_FromPlaneCoords((Vec2){ 0.0f, 0.0f }, axis, offset));
    return true;
}

static bool add_plane(Layout* layout, const cJSON* item, AgentSceneCounts* counts) {
    PlanePrimitiveCreateParams params;
    Vec3 position = {0};
    ViewPlaneAxis axis = VIEW_PLANE_XY;
    bool bounds_adjusted = false;
    uint32_t object_id = 0u;

    if (!layout || !item) return false;
    if (!parse_vec3_required(item, "position", &position)) return false;
    if (!parse_axis(json_string_or(item, "axis", "xy"), &axis)) return false;

    Layout_PlanePrimitiveCreateParams_SetDefaults(&params);
    params.width = (float)json_number_or(item, "width", 4.0);
    params.height = (float)json_number_or(item, "height", 4.0);
    params.lockToConstructionPlane = json_bool_or(item, "lock_to_construction_plane", false);
    params.lockToBounds = json_bool_or(item, "lock_to_bounds", true);
    params.useExplicitFrame = true;
    params.explicitFrame = frame_from_axis(axis, position);

    if (!Layout_CreatePlanePrimitive(layout, &params, &object_id, &bounds_adjusted)) {
        return false;
    }
    Object3D* object = Layout_ObjectStore_Find(&layout->objectStore, object_id);
    if (!object) return false;
    if (!apply_request_object_id(object, item)) return false;
    object->coreMeta.dimensional_mode = CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED;
    object->coreMeta.locked_plane = core_plane_from_axis(axis);
    if (counts) {
        counts->planes += 1;
        if (bounds_adjusted) counts->bounds_adjusted += 1;
    }
    return true;
}

static bool add_rect_prism(Layout* layout, const cJSON* item, AgentSceneCounts* counts) {
    RectPrismPrimitiveCreateParams params;
    Vec3 position = {0};
    ViewPlaneAxis axis = VIEW_PLANE_XY;
    bool bounds_adjusted = false;
    uint32_t object_id = 0u;

    if (!layout || !item) return false;
    if (!parse_vec3_required(item, "position", &position)) return false;
    if (!parse_axis(json_string_or(item, "axis", "xy"), &axis)) return false;

    Layout_RectPrismPrimitiveCreateParams_SetDefaults(&params);
    params.width = (float)json_number_or(item, "width", 1.0);
    params.height = (float)json_number_or(item, "height", 1.0);
    params.depth = (float)json_number_or(item, "depth", 1.0);
    params.lockToConstructionPlane = json_bool_or(item, "lock_to_construction_plane", false);
    params.lockToBounds = json_bool_or(item, "lock_to_bounds", true);
    params.useExplicitFrame = true;
    params.explicitFrame = frame_from_axis(axis, position);

    if (!Layout_CreateRectPrismPrimitive(layout, &params, &object_id, &bounds_adjusted)) {
        return false;
    }
    Object3D* object = Layout_ObjectStore_Find(&layout->objectStore, object_id);
    if (!object || !apply_request_object_id(object, item)) return false;
    if (counts) {
        counts->rect_prisms += 1;
        if (bounds_adjusted) counts->bounds_adjusted += 1;
    }
    return true;
}

static bool append_inline_physics_overlay(cJSON* overlays, const cJSON* item) {
    const cJSON* physics_sim = NULL;
    const char* object_id = NULL;
    cJSON* overlay = NULL;
    if (!overlays || !item) return false;
    physics_sim = cJSON_GetObjectItemCaseSensitive(item, "physics_sim");
    if (!cJSON_IsObject(physics_sim)) return true;
    object_id = json_string_or(item, "id", NULL);
    if (!object_id) return false;

    overlay = cJSON_Duplicate(physics_sim, 1);
    if (!overlay) return false;
    cJSON_DeleteItemFromObjectCaseSensitive(overlay, "object_id");
    cJSON_AddStringToObject(overlay, "object_id", object_id);
    cJSON_AddItemToArray(overlays, overlay);
    return true;
}

static bool apply_request_extensions_to_authoring(const char* authoring_path, const cJSON* request) {
    char* text = NULL;
    char* updated = NULL;
    cJSON* authoring = NULL;
    cJSON* extensions = NULL;
    cJSON* physics_sim = NULL;
    cJSON* overlays = NULL;
    const cJSON* request_extensions = NULL;
    const cJSON* request_physics = NULL;
    const cJSON* objects = NULL;
    bool ok = false;

    if (!authoring_path || !request) return false;
    if (!read_text_file(authoring_path, &text)) goto done;
    authoring = cJSON_Parse(text);
    if (!cJSON_IsObject(authoring)) goto done;

    extensions = ensure_object_member(authoring, "extensions");
    if (!extensions) goto done;

    request_extensions = cJSON_GetObjectItemCaseSensitive(request, "extensions");
    if (cJSON_IsObject(request_extensions) && !merge_object_members(extensions, request_extensions)) {
        goto done;
    }

    request_physics = cJSON_GetObjectItemCaseSensitive(request, "physics_sim");
    if (cJSON_IsObject(request_physics)) {
        physics_sim = ensure_object_member(extensions, "physics_sim");
        if (!physics_sim || !merge_object_members(physics_sim, request_physics)) goto done;
    } else {
        physics_sim = cJSON_GetObjectItemCaseSensitive(extensions, "physics_sim");
    }

    objects = cJSON_GetObjectItemCaseSensitive(request, "objects");
    if (cJSON_IsArray(objects)) {
        cJSON* item = NULL;
        cJSON_ArrayForEach(item, objects) {
            if (!cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(item, "physics_sim"))) continue;
            physics_sim = ensure_object_member(extensions, "physics_sim");
            if (!physics_sim) goto done;
            overlays = ensure_array_member(physics_sim, "object_overlays");
            if (!overlays || !append_inline_physics_overlay(overlays, item)) goto done;
        }
    }

    if (!AgentSceneMaterialFlow_ApplyObjectPrompts(authoring, request)) goto done;

    updated = cJSON_Print(authoring);
    if (!updated) goto done;
    ok = write_text_file(authoring_path, updated);

done:
    free(text);
    free(updated);
    cJSON_Delete(authoring);
    return ok;
}

static bool build_layout_from_request(const cJSON* root, Layout* layout, AgentSceneCounts* counts) {
    const cJSON* objects = NULL;
    if (!root || !layout) return false;
    memset(counts, 0, sizeof(*counts));
    Layout_Init(layout, 1.0f);
    layout->gridSize = (float)json_number_or(root, "grid_size", 1.0);
    if (!parse_bounds(root, &layout->scene3d.bounds)) return false;
    if (!apply_construction_plane(root, layout)) return false;

    objects = cJSON_GetObjectItemCaseSensitive(root, "objects");
    if (!cJSON_IsArray(objects)) return false;

    cJSON* item = NULL;
    cJSON_ArrayForEach(item, objects) {
        const char* kind = json_string_or(item, "kind", "");
        if (strcmp(kind, "plane") == 0) {
            if (!add_plane(layout, item, counts)) return false;
        } else if (strcmp(kind, "rect_prism") == 0) {
            if (!add_rect_prism(layout, item, counts)) return false;
        } else if (strcmp(kind, "mesh_asset_instance") == 0) {
            if (counts) counts->mesh_asset_instances += 1;
        } else {
            return false;
        }
    }
    return true;
}

static cJSON* create_mesh_asset_authoring_object(const cJSON* item) {
    const char* object_id = json_string_or(item, "id", NULL);
    const char* asset_id = mesh_asset_id_from_request(item);
    const char* variant = json_string_or(item, "variant", "runtime_default");
    const char* material_id = json_string_or(item, "material_id", NULL);
    Vec3 position = {0};
    Vec3 rotation = {0.0f, 0.0f, 0.0f};
    Vec3 scale = {1.0f, 1.0f, 1.0f};
    cJSON* object_json = NULL;
    cJSON* transform = NULL;
    cJSON* geometry_ref = NULL;
    cJSON* material_ref = NULL;
    cJSON* flags = NULL;
    cJSON* tags = NULL;

    if (!object_id || !object_id[0] || !asset_id || !asset_id[0]) return NULL;
    if (!parse_vec3_required(item, "position", &position)) return NULL;
    if (!parse_vec3_optional(item, "rotation", rotation, &rotation)) return NULL;
    if (!parse_vec3_optional(item, "scale", scale, &scale)) return NULL;

    object_json = cJSON_CreateObject();
    transform = cJSON_CreateObject();
    geometry_ref = cJSON_CreateObject();
    flags = cJSON_CreateObject();
    tags = cJSON_CreateArray();
    if (!object_json || !transform || !geometry_ref || !flags || !tags) goto fail;

    cJSON_AddStringToObject(object_json, "object_id", object_id);
    cJSON_AddStringToObject(object_json, "object_type", "mesh_asset_instance");
    cJSON_AddStringToObject(object_json, "space_mode_intent", "3d");
    cJSON_AddStringToObject(object_json, "dimensional_mode", "full_3d");
    cJSON_AddItemToObject(object_json, "transform", transform);
    transform = NULL;
    if (!add_vec3_json(cJSON_GetObjectItemCaseSensitive(object_json, "transform"),
                       "position",
                       position) ||
        !add_vec3_json(cJSON_GetObjectItemCaseSensitive(object_json, "transform"),
                       "rotation",
                       rotation) ||
        !add_vec3_json(cJSON_GetObjectItemCaseSensitive(object_json, "transform"),
                       "scale",
                       scale)) {
        goto fail;
    }
    cJSON_AddItemToObject(object_json, "geometry_ref", geometry_ref);
    cJSON_AddStringToObject(geometry_ref, "kind", "mesh_asset");
    cJSON_AddStringToObject(geometry_ref, "id", asset_id);
    cJSON_AddStringToObject(geometry_ref, "variant", variant && variant[0] ? variant : "runtime_default");
    geometry_ref = NULL;
    if (material_id && material_id[0]) {
        material_ref = cJSON_CreateObject();
        if (!material_ref) goto fail;
        cJSON_AddItemToObject(object_json, "material_ref", material_ref);
        cJSON_AddStringToObject(material_ref, "id", material_id);
        material_ref = NULL;
    }
    cJSON_AddItemToObject(object_json, "tags", tags);
    cJSON_AddItemToArray(tags, cJSON_CreateString("authoring"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("line_drawing"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("mesh_asset"));
    tags = NULL;
    cJSON_AddItemToObject(object_json, "flags", flags);
    cJSON_AddBoolToObject(flags, "visible", json_bool_or(item, "visible", true));
    cJSON_AddBoolToObject(flags, "locked", json_bool_or(item, "locked", false));
    cJSON_AddBoolToObject(flags, "selectable", json_bool_or(item, "selectable", true));
    flags = NULL;
    return object_json;

fail:
    cJSON_Delete(object_json);
    cJSON_Delete(transform);
    cJSON_Delete(geometry_ref);
    cJSON_Delete(material_ref);
    cJSON_Delete(flags);
    cJSON_Delete(tags);
    return NULL;
}

static bool apply_request_mesh_assets_to_authoring(const char* authoring_path, const cJSON* request) {
    char* text = NULL;
    char* updated = NULL;
    cJSON* authoring = NULL;
    cJSON* objects = NULL;
    const cJSON* request_objects = NULL;
    bool ok = false;

    if (!authoring_path || !request) return false;
    if (!read_text_file(authoring_path, &text)) goto done;
    authoring = cJSON_Parse(text);
    if (!cJSON_IsObject(authoring)) goto done;
    objects = cJSON_GetObjectItemCaseSensitive(authoring, "objects");
    if (!cJSON_IsArray(objects)) goto done;

    request_objects = cJSON_GetObjectItemCaseSensitive(request, "objects");
    if (cJSON_IsArray(request_objects)) {
        const cJSON* item = NULL;
        cJSON_ArrayForEach(item, request_objects) {
            cJSON* mesh_object = NULL;
            const char* kind = json_string_or(item, "kind", "");
            if (strcmp(kind, "mesh_asset_instance") != 0) continue;
            if (!ensure_mesh_material(authoring, item)) goto done;
            mesh_object = create_mesh_asset_authoring_object(item);
            if (!mesh_object) goto done;
            cJSON_AddItemToArray(objects, mesh_object);
        }
    }

    updated = cJSON_Print(authoring);
    if (!updated) goto done;
    ok = write_text_file(authoring_path, updated);

done:
    free(text);
    free(updated);
    cJSON_Delete(authoring);
    return ok;
}

static bool copy_mesh_asset_to_scene_dir(const char* source_path,
                                         const char* scene_dir,
                                         const char* asset_id) {
    char assets_dir[AGENT_SCENE_PATH_MAX];
    char mesh_assets_dir[AGENT_SCENE_PATH_MAX];
    char asset_filename[256];
    char dest_path[AGENT_SCENE_PATH_MAX];
    if (!source_path || !scene_dir || !asset_id || !asset_id[0]) return false;
    if (snprintf(asset_filename, sizeof(asset_filename), "%s.runtime.json", asset_id) >=
        (int)sizeof(asset_filename)) {
        return false;
    }
    if (!join_path(assets_dir, sizeof(assets_dir), scene_dir, "assets")) return false;
    if (!join_path(mesh_assets_dir, sizeof(mesh_assets_dir), assets_dir, "mesh_assets")) {
        return false;
    }
    if (!ensure_dir(mesh_assets_dir)) return false;
    if (!join_path(dest_path, sizeof(dest_path), mesh_assets_dir, asset_filename)) return false;
    return copy_text_file(source_path, dest_path);
}

static bool copy_request_mesh_assets(const cJSON* request,
                                     const char* request_path,
                                     const char* output_dir,
                                     const char* app_load_dir,
                                     AgentSceneCounts* counts) {
    const cJSON* request_objects = cJSON_GetObjectItemCaseSensitive(request, "objects");
    const cJSON* item = NULL;
    if (!cJSON_IsArray(request_objects)) return false;
    cJSON_ArrayForEach(item, request_objects) {
        const char* kind = json_string_or(item, "kind", "");
        const char* asset_id = NULL;
        const char* source_path = NULL;
        char resolved_source[AGENT_SCENE_PATH_MAX];
        if (strcmp(kind, "mesh_asset_instance") != 0) continue;
        asset_id = mesh_asset_id_from_request(item);
        source_path = json_string_or(item, "asset_source_path", NULL);
        if (!source_path) source_path = json_string_or(item, "source_path", NULL);
        if (!asset_id || !asset_id[0] || !source_path || !source_path[0]) return false;
        if (!resolve_request_relative_path(resolved_source,
                                           sizeof(resolved_source),
                                           request_path,
                                           source_path)) {
            return false;
        }
        if (!copy_mesh_asset_to_scene_dir(resolved_source, output_dir, asset_id)) return false;
        if (app_load_dir && app_load_dir[0] &&
            !copy_mesh_asset_to_scene_dir(resolved_source, app_load_dir, asset_id)) {
            return false;
        }
        if (counts) counts->mesh_assets_copied += 1;
    }
    return true;
}

static bool compile_runtime_file(const char* authoring_path, const char* runtime_path) {
    char diagnostics[512];
    memset(diagnostics, 0, sizeof(diagnostics));
    CoreResult r = core_scene_compile_authoring_file_to_runtime_file(
        authoring_path,
        runtime_path,
        diagnostics,
        sizeof(diagnostics));
    if (r.code != CORE_OK) {
        fprintf(stderr,
                "[agent_scene_tool] ERROR: scene compile failed: %s\n",
                diagnostics[0] ? diagnostics : "(no diagnostics)");
        return false;
    }
    return true;
}

static bool run_determinism_check(const char* runtime_path, const char* second_runtime_path) {
    char* first = NULL;
    char* second = NULL;
    bool ok = false;
    if (!read_text_file(runtime_path, &first)) goto done;
    if (!read_text_file(second_runtime_path, &second)) goto done;
    ok = strcmp(first, second) == 0;
done:
    free(first);
    free(second);
    return ok;
}

static bool write_summary(const char* path,
                          const char* scene_id,
                          const char* layout_path,
                          const char* authoring_path,
                          const char* runtime_path,
                          const char* app_layout_path,
                          const char* app_authoring_path,
                          const char* app_runtime_path,
                          const AgentSceneCounts* counts) {
    cJSON* root = cJSON_CreateObject();
    cJSON* files = cJSON_CreateObject();
    cJSON* object_counts = cJSON_CreateObject();
    char* text = NULL;
    bool ok = false;
    if (!root || !files || !object_counts || !counts) goto done;

    cJSON_AddStringToObject(root, "schema", "line_drawing_agent_scene_summary_v1");
    cJSON_AddStringToObject(root, "scene_id", scene_id ? scene_id : "scene_line_drawing_agent");
    cJSON_AddStringToObject(files, "layout", layout_path);
    cJSON_AddStringToObject(files, "authoring", authoring_path);
    cJSON_AddStringToObject(files, "runtime", runtime_path);
    cJSON_AddStringToObject(files, "app_layout", app_layout_path);
    cJSON_AddStringToObject(files, "app_authoring", app_authoring_path);
    cJSON_AddStringToObject(files, "app_runtime", app_runtime_path);
    cJSON_AddItemToObject(root, "files", files);
    files = NULL;
    cJSON_AddNumberToObject(object_counts, "planes", counts->planes);
    cJSON_AddNumberToObject(object_counts, "rect_prisms", counts->rect_prisms);
    cJSON_AddNumberToObject(object_counts, "mesh_asset_instances", counts->mesh_asset_instances);
    cJSON_AddNumberToObject(object_counts, "mesh_assets_copied", counts->mesh_assets_copied);
    cJSON_AddNumberToObject(object_counts, "bounds_adjusted", counts->bounds_adjusted);
    cJSON_AddItemToObject(root, "object_counts", object_counts);
    object_counts = NULL;

    text = cJSON_Print(root);
    if (!text) goto done;
    ok = write_text_file(path, text);

done:
    cJSON_Delete(root);
    cJSON_Delete(files);
    cJSON_Delete(object_counts);
    free(text);
    return ok;
}

int main(int argc, char** argv) {
    AgentSceneOptions opts;
    AgentSceneCounts counts;
    char* request_text = NULL;
    cJSON* request = NULL;
    Layout layout;
    bool layout_initialized = false;
    char request_copy_path[AGENT_SCENE_PATH_MAX];
    char layout_path[AGENT_SCENE_PATH_MAX];
    char authoring_path[AGENT_SCENE_PATH_MAX];
    char runtime_path[AGENT_SCENE_PATH_MAX];
    char runtime_second_path[AGENT_SCENE_PATH_MAX];
    char summary_path[AGENT_SCENE_PATH_MAX];
    char app_run_dir[AGENT_SCENE_PATH_MAX];
    char app_layout_path[AGENT_SCENE_PATH_MAX];
    char app_load_dir[AGENT_SCENE_PATH_MAX];
    char app_authoring_path[AGENT_SCENE_PATH_MAX];
    char app_runtime_path[AGENT_SCENE_PATH_MAX];
    const char* scene_id = NULL;
    const cJSON* scene_options = NULL;

    if (!parse_args(argc, argv, &opts)) {
        print_usage(argv[0]);
        return 2;
    }
    if (!ensure_dir(opts.output_dir)) {
        fprintf(stderr, "[agent_scene_tool] ERROR: failed to create output directory: %s\n", opts.output_dir);
        return 1;
    }
    if (!join_path(request_copy_path, sizeof(request_copy_path), opts.output_dir, "agent_request.json") ||
        !join_path(layout_path, sizeof(layout_path), opts.output_dir, "layout.json") ||
        !join_path(authoring_path, sizeof(authoring_path), opts.output_dir, "scene_authoring.json") ||
        !join_path(runtime_path, sizeof(runtime_path), opts.output_dir, "scene_runtime.json") ||
        !join_path(runtime_second_path, sizeof(runtime_second_path), opts.output_dir, "scene_runtime.determinism.second.json") ||
        !join_path(summary_path, sizeof(summary_path), opts.output_dir, "scene_summary.json")) {
        fprintf(stderr, "[agent_scene_tool] ERROR: output path too long\n");
        return 1;
    }

    if (!read_text_file(opts.request_path, &request_text)) {
        fprintf(stderr, "[agent_scene_tool] ERROR: failed to read request: %s\n", opts.request_path);
        return 1;
    }
    request = cJSON_Parse(request_text);
    if (!cJSON_IsObject(request)) {
        fprintf(stderr, "[agent_scene_tool] ERROR: request is not a JSON object\n");
        free(request_text);
        cJSON_Delete(request);
        return 1;
    }
    if (strcmp(json_string_or(request, "schema", ""), "line_drawing_agent_scene_request_v1") != 0) {
        fprintf(stderr, "[agent_scene_tool] ERROR: unsupported request schema\n");
        free(request_text);
        cJSON_Delete(request);
        return 1;
    }

    scene_id = json_string_or(request, "scene_id", "scene_line_drawing_agent");
    if (!build_layout_from_request(request, &layout, &counts)) {
        fprintf(stderr, "[agent_scene_tool] ERROR: failed to build layout from request\n");
        free(request_text);
        cJSON_Delete(request);
        return 1;
    }
    layout_initialized = true;
    if (!derive_app_load_paths(opts.output_dir,
                               scene_id,
                               app_run_dir,
                               sizeof(app_run_dir),
                               app_layout_path,
                               sizeof(app_layout_path),
                               app_load_dir,
                               sizeof(app_load_dir),
                               app_authoring_path,
                               sizeof(app_authoring_path),
                               app_runtime_path,
                               sizeof(app_runtime_path))) {
        fprintf(stderr, "[agent_scene_tool] ERROR: app-loadable output path too long\n");
        goto fail;
    }
    if (!ensure_dir(app_load_dir)) {
        fprintf(stderr, "[agent_scene_tool] ERROR: failed to create app-loadable scene directory: %s\n", app_load_dir);
        goto fail;
    }

    scene_options = cJSON_GetObjectItemCaseSensitive(request, "scene_options");
    LineDrawingSceneAuthoringOptions authoring_options = {
        .material_id = json_string_or(scene_options, "material_id", NULL),
        .material_type = json_string_or(scene_options, "material_type", NULL),
        .light_id = json_string_or(scene_options, "light_id", NULL),
        .light_type = json_string_or(scene_options, "light_type", NULL),
        .camera_id = json_string_or(scene_options, "camera_id", NULL),
        .camera_type = json_string_or(scene_options, "camera_type", NULL),
        .preview_mode = json_string_or(scene_options, "preview_mode", NULL),
    };

    if (!write_text_file(request_copy_path, request_text)) {
        fprintf(stderr, "[agent_scene_tool] ERROR: failed to copy request into output directory\n");
        goto fail;
    }
    if (!Layout_SaveToFile(&layout, layout_path)) {
        fprintf(stderr, "[agent_scene_tool] ERROR: failed to write layout: %s\n", layout_path);
        goto fail;
    }
    if (!LineDrawingCanonicalScene_ExportLayoutToFileWithOptions(&layout,
                                                                 scene_id,
                                                                 authoring_path,
                                                                 &authoring_options)) {
        fprintf(stderr, "[agent_scene_tool] ERROR: failed to export authoring scene\n");
        goto fail;
    }
    if (!apply_request_mesh_assets_to_authoring(authoring_path, request)) {
        fprintf(stderr, "[agent_scene_tool] ERROR: failed to apply request mesh assets\n");
        goto fail;
    }
    if (!apply_request_extensions_to_authoring(authoring_path, request)) {
        fprintf(stderr, "[agent_scene_tool] ERROR: failed to apply request scene extensions\n");
        goto fail;
    }
    if (!compile_runtime_file(authoring_path, runtime_path)) goto fail;
    if (opts.determinism_check) {
        if (!compile_runtime_file(authoring_path, runtime_second_path)) goto fail;
        if (!run_determinism_check(runtime_path, runtime_second_path)) {
            fprintf(stderr, "[agent_scene_tool] ERROR: deterministic compile check failed\n");
            goto fail;
        }
        remove(runtime_second_path);
    }
    if (!copy_text_file(layout_path, app_layout_path)) {
        fprintf(stderr, "[agent_scene_tool] ERROR: failed to write app-loadable layout: %s\n", app_layout_path);
        goto fail;
    }
    if (!copy_text_file(authoring_path, app_authoring_path) ||
        !copy_text_file(runtime_path, app_runtime_path)) {
        fprintf(stderr, "[agent_scene_tool] ERROR: failed to write app-loadable scene files\n");
        goto fail;
    }
    if (!copy_request_mesh_assets(request, opts.request_path, opts.output_dir, app_load_dir, &counts)) {
        fprintf(stderr, "[agent_scene_tool] ERROR: failed to copy request mesh assets\n");
        goto fail;
    }
    if (!write_summary(summary_path,
                       scene_id,
                       layout_path,
                       authoring_path,
                       runtime_path,
                       app_layout_path,
                       app_authoring_path,
                       app_runtime_path,
                       &counts)) {
        fprintf(stderr, "[agent_scene_tool] ERROR: failed to write summary\n");
        goto fail;
    }

    printf("[agent_scene_tool] PASS\n");
    printf("[agent_scene_tool] layout:    %s\n", layout_path);
    printf("[agent_scene_tool] authoring: %s\n", authoring_path);
    printf("[agent_scene_tool] runtime:   %s\n", runtime_path);
    printf("[agent_scene_tool] app json:  %s\n", app_layout_path);
    printf("[agent_scene_tool] app scene: %s\n", app_load_dir);
    printf("[agent_scene_tool] summary:   %s\n", summary_path);

    if (layout_initialized) Layout_Free(&layout);
    free(request_text);
    cJSON_Delete(request);
    return 0;

fail:
    if (layout_initialized) Layout_Free(&layout);
    free(request_text);
    cJSON_Delete(request);
    return 1;
}
