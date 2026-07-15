#include "Tools/canonical_scene_export.h"

#include "Layout/layout_json.h"
#include "core_io.h"
#include "core_scene.h"
#include "core_object.h"
#include "core_units.h"
#include "Tools/canonical_scene_export_authoring.h"
#include "Tools/canonical_scene_export_materials.h"
#include "Tools/canonical_scene_export_primitives.h"
#include "cjson/cJSON.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* kSchemaFamily = "codework_scene";
static const char* kSchemaVariant = "scene_authoring_v1";
static const int kSchemaVersion = 1;
static const char* kDefaultSceneId = "scene_line_drawing";
static const char* kDefaultObjectId = "obj_line_drawing_layout";
static const char* kDefaultGeometryId = "shape_line_drawing_layout";
static const char* kAnchorSetObjectId = "obj_line_drawing_anchor_set";
static const char* kAnchorSetGeometryId = "shape_line_drawing_anchor_set";
static const char* kWallSetObjectId = "obj_line_drawing_wall_set";
static const char* kWallSetGeometryId = "shape_line_drawing_wall_set";
static const char* kDefaultMaterialId = "mat_line_drawing_default";
static const char* kDefaultLightId = "light_line_drawing_key";
static const char* kDefaultCameraId = "cam_line_drawing_default";
static const char* kDefaultCameraPathLabel = "Camera Path";
static const char* kDefaultLightPathLabel = "Light Path";
static const char* kDefaultPreviewMode = "wireframe";
static const char* kDefaultMaterialType = "flat_color";
static const char* kDefaultLightType = "directional";
static const char* kDefaultUnitSystem = "meters";
static const char* kConversionPolicy = "explicit_only";
static const double kDefaultWorldScale = 1.0;

static const char* string_or_default(const char* value, const char* fallback) {
    if (!value || !value[0]) return fallback;
    return value;
}

static bool is_valid_token(const char* value) {
    if (!value || !value[0]) return false;
    for (const char* p = value; *p; ++p) {
        const char c = *p;
        const bool ok = (c >= 'a' && c <= 'z') ||
                        (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') ||
                        c == '_' || c == '-' || c == '.';
        if (!ok) return false;
    }
    return true;
}

static cJSON* vec3_to_json_object(Vec3 value) {
    cJSON* node = cJSON_CreateObject();
    if (!node) return NULL;
    cJSON_AddNumberToObject(node, "x", value.x);
    cJSON_AddNumberToObject(node, "y", value.y);
    cJSON_AddNumberToObject(node, "z", value.z);
    return node;
}

static bool append_path_control_point(cJSON* control_points, Vec3 position) {
    cJSON* point = NULL;
    if (!control_points) return false;
    point = vec3_to_json_object(position);
    if (!point) return false;
    cJSON_AddItemToArray(control_points, point);
    return true;
}

static bool append_scene_path(cJSON* paths,
                              const char* path_id,
                              const char* path_kind,
                              const char* label,
                              const char* bound_key,
                              const char* bound_id,
                              Vec3 start,
                              Vec3 control,
                              Vec3 end) {
    cJSON* path = NULL;
    cJSON* control_points = NULL;
    if (!paths || !path_id || !path_id[0] || !path_kind || !path_kind[0]) return false;

    path = cJSON_CreateObject();
    control_points = cJSON_CreateArray();
    if (!path || !control_points) {
        cJSON_Delete(path);
        cJSON_Delete(control_points);
        return false;
    }

    cJSON_AddStringToObject(path, "path_id", path_id);
    cJSON_AddStringToObject(path, "path_kind", path_kind);
    cJSON_AddStringToObject(path, "label", string_or_default(label, path_id));
    cJSON_AddStringToObject(path, "curve_type", "bezier");
    if (bound_key && bound_key[0] && bound_id && bound_id[0]) {
        cJSON_AddStringToObject(path, bound_key, bound_id);
    }
    cJSON_AddItemToObject(path, "control_points", control_points);

    if (!append_path_control_point(control_points, start) ||
        !append_path_control_point(control_points, control) ||
        !append_path_control_point(control_points, end)) {
        cJSON_Delete(path);
        return false;
    }

    cJSON_AddItemToArray(paths, path);
    return true;
}

static bool is_allowed_material_type(const char* value) {
    return strcmp(value, "flat_color") == 0;
}

static bool is_allowed_light_type(const char* value) {
    return strcmp(value, "directional") == 0 ||
           strcmp(value, "point") == 0 ||
           strcmp(value, "spot") == 0;
}

static bool is_allowed_camera_type(const char* value) {
    return strcmp(value, "perspective") == 0 ||
           strcmp(value, "orthographic") == 0;
}

static bool is_allowed_preview_mode(const char* value) {
    return strcmp(value, "bounds") == 0 ||
           strcmp(value, "wireframe") == 0 ||
           strcmp(value, "flat") == 0 ||
           strcmp(value, "material") == 0;
}

static bool is_allowed_unit_system(const char* value) {
    return strcmp(value, kDefaultUnitSystem) == 0;
}

static bool is_allowed_conversion_policy(const char* value) {
    return strcmp(value, kConversionPolicy) == 0;
}

static double scene_authoring_world_scale_or_default(const LineDrawingSceneAuthoringOptions* options) {
    if (!options || !(options->world_scale > 0.0)) return kDefaultWorldScale;
    return options->world_scale;
}

static bool scene_authoring_options_override_live_records(
    const LineDrawingSceneAuthoringOptions* options) {
    if (!options) return false;
    return (options->material_id && options->material_id[0]) ||
           (options->light_id && options->light_id[0]) ||
           (options->camera_id && options->camera_id[0]) ||
           (options->camera_path_id && options->camera_path_id[0]) ||
           (options->light_path_id && options->light_path_id[0]) ||
           options->material_binding_count > 0u;
}

static bool validate_options(const LineDrawingSceneAuthoringOptions* options) {
    if (!options) return true;

    if (options->material_id && !is_valid_token(options->material_id)) return false;
    if (options->light_id && !is_valid_token(options->light_id)) return false;
    if (options->camera_id && !is_valid_token(options->camera_id)) return false;
    if (options->camera_path_id && !is_valid_token(options->camera_path_id)) return false;
    if (options->light_path_id && !is_valid_token(options->light_path_id)) return false;

    if (options->material_type && !is_allowed_material_type(options->material_type)) return false;
    if (options->material_binding_count > 0u) {
        if (!options->material_bindings) return false;
        for (size_t i = 0u; i < options->material_binding_count; ++i) {
            if (!LineDrawingCanonicalScene_ValidateMaterialBindingOption(
                    &options->material_bindings[i])) {
                return false;
            }
        }
    }
    if (options->light_type && !is_allowed_light_type(options->light_type)) return false;
    if (options->camera_type && !is_allowed_camera_type(options->camera_type)) return false;
    if (options->preview_mode && !is_allowed_preview_mode(options->preview_mode)) return false;
    if (options->unit_system && !is_allowed_unit_system(options->unit_system)) return false;
    if (options->conversion_policy &&
        !is_allowed_conversion_policy(options->conversion_policy)) {
        return false;
    }
    if (core_units_validate_world_scale(scene_authoring_world_scale_or_default(options)).code != CORE_OK) {
        return false;
    }
    return true;
}

static const char* get_existing_scene_id(const cJSON* existing_root) {
    const cJSON* scene_id = NULL;
    if (!existing_root) return NULL;
    scene_id = cJSON_GetObjectItemCaseSensitive(existing_root, "scene_id");
    if (!cJSON_IsString(scene_id) || !scene_id->valuestring || !scene_id->valuestring[0]) {
        return NULL;
    }
    return scene_id->valuestring;
}

static size_t count_active_anchors(const Layout* layout) {
    size_t count = 0;
    if (!layout) return 0;
    for (size_t i = 0; i < layout->anchorCount; ++i) {
        if (!layout->anchors[i].isDeleted) ++count;
    }
    return count;
}

static size_t count_active_walls(const Layout* layout) {
    size_t count = 0;
    if (!layout) return 0;
    for (size_t i = 0; i < layout->wallCount; ++i) {
        if (!layout->walls[i].isDeleted) ++count;
    }
    return count;
}

static size_t count_live_object3d(const Layout* layout) {
    size_t count = 0;
    if (!layout) return 0;
    for (size_t i = 0; i < layout->objectStore.count; ++i) {
        if (!layout->objectStore.items[i].isDeleted) ++count;
    }
    return count;
}

static bool vec3_nearly_equal(Vec3 a, Vec3 b) {
    return fabsf(a.x - b.x) <= 0.0001f &&
           fabsf(a.y - b.y) <= 0.0001f &&
           fabsf(a.z - b.z) <= 0.0001f;
}

static Vec3 scene_bounds_center(SceneBounds3D bounds) {
    return (Vec3){
        .x = 0.5f * (bounds.min.x + bounds.max.x),
        .y = 0.5f * (bounds.min.y + bounds.max.y),
        .z = 0.5f * (bounds.min.z + bounds.max.z)
    };
}

static bool layout_has_scene3d_authoring(const Layout* layout) {
    Scene3DSettings defaults;
    if (!layout) return false;
    if (count_live_object3d(layout) > 0u) return true;

    Layout_Scene3DSettings_SetDefaults(&defaults);
    if (layout->scene3d.bounds.enabled != defaults.bounds.enabled) return true;
    if (layout->scene3d.bounds.clampOnEdit != defaults.bounds.clampOnEdit) return true;
    if (!vec3_nearly_equal(layout->scene3d.bounds.min, defaults.bounds.min)) return true;
    if (!vec3_nearly_equal(layout->scene3d.bounds.max, defaults.bounds.max)) return true;
    if (layout->scene3d.constructionPlane.mode != defaults.constructionPlane.mode) return true;
    if (layout->scene3d.constructionPlane.axisAligned.axis != defaults.constructionPlane.axisAligned.axis) return true;
    if (fabsf(layout->scene3d.constructionPlane.axisAligned.offset -
              defaults.constructionPlane.axisAligned.offset) > 0.0001f) {
        return true;
    }
    if (!vec3_nearly_equal(layout->scene3d.constructionPlane.customFrame.origin,
                           defaults.constructionPlane.customFrame.origin)) {
        return true;
    }
    if (!vec3_nearly_equal(layout->scene3d.constructionPlane.customFrame.axisU,
                           defaults.constructionPlane.customFrame.axisU)) {
        return true;
    }
    if (!vec3_nearly_equal(layout->scene3d.constructionPlane.customFrame.axisV,
                           defaults.constructionPlane.customFrame.axisV)) {
        return true;
    }
    if (!vec3_nearly_equal(layout->scene3d.constructionPlane.customFrame.normal,
                           defaults.constructionPlane.customFrame.normal)) {
        return true;
    }
    return false;
}

static bool layout_uses_3d(const Layout* layout) {
    if (!layout) return false;
    for (size_t i = 0; i < layout->anchorCount; ++i) {
        const Anchor* anchor = &layout->anchors[i];
        if (anchor->isDeleted) continue;
        if (fabs(anchor->pos.z) > 0.0001f) return true;
    }
    if (layout_has_scene3d_authoring(layout)) return true;
    return false;
}

static cJSON* duplicate_or_empty_object(const cJSON* source) {
    if (!source || !cJSON_IsObject(source)) {
        return cJSON_CreateObject();
    }
    return cJSON_Duplicate(source, 1);
}

static bool upsert_object_item(cJSON* object, const char* key, cJSON* item);

static bool add_layout_snapshot_extension(cJSON* line_drawing_ext, const Layout* layout) {
    cJSON* layout_snapshot = NULL;
    char* layout_json = NULL;

    if (!line_drawing_ext || !layout) return false;

    layout_json = Layout_SaveToString(layout);
    if (!layout_json) return false;

    layout_snapshot = cJSON_Parse(layout_json);
    Layout_FreeString(layout_json);
    if (!cJSON_IsObject(layout_snapshot)) {
        cJSON_Delete(layout_snapshot);
        return false;
    }

    if (!cJSON_ReplaceItemInObjectCaseSensitive(line_drawing_ext, "layout_snapshot", layout_snapshot)) {
        cJSON_AddItemToObject(line_drawing_ext, "layout_snapshot", layout_snapshot);
    }
    return true;
}

static bool add_object_tag(cJSON* tags, const char* tag) {
    if (!tags || !tag) return false;
    cJSON_AddItemToArray(tags, cJSON_CreateString(tag));
    return true;
}

static bool add_object_payload(cJSON* object_json,
                               const CoreObject* object,
                               const char* geometry_id,
                               const char* material_id,
                               const char* tag2,
                               const char* tag3) {
    cJSON* transform = NULL;
    cJSON* position = NULL;
    cJSON* rotation = NULL;
    cJSON* scale = NULL;
    cJSON* geometry_ref = NULL;
    cJSON* material_ref = NULL;
    cJSON* tags = NULL;
    cJSON* flags = NULL;

    if (!object_json || !object || !geometry_id) return false;

    transform = cJSON_CreateObject();
    position = cJSON_CreateObject();
    rotation = cJSON_CreateObject();
    scale = cJSON_CreateObject();
    geometry_ref = cJSON_CreateObject();
    tags = cJSON_CreateArray();
    flags = cJSON_CreateObject();
    if (!transform || !position || !rotation || !scale || !geometry_ref || !tags || !flags) {
        return false;
    }

    cJSON_AddItemToObject(object_json, "transform", transform);
    cJSON_AddItemToObject(transform, "position", position);
    cJSON_AddItemToObject(transform, "rotation", rotation);
    cJSON_AddItemToObject(transform, "scale", scale);
    cJSON_AddItemToObject(object_json, "geometry_ref", geometry_ref);
    cJSON_AddItemToObject(object_json, "tags", tags);
    cJSON_AddItemToObject(object_json, "flags", flags);

    cJSON_AddNumberToObject(position, "x", object->transform.position.x);
    cJSON_AddNumberToObject(position, "y", object->transform.position.y);
    cJSON_AddNumberToObject(position, "z", object->transform.position.z);

    cJSON_AddNumberToObject(rotation, "x", object->transform.rotation_deg.x);
    cJSON_AddNumberToObject(rotation, "y", object->transform.rotation_deg.y);
    cJSON_AddNumberToObject(rotation, "z", object->transform.rotation_deg.z);

    cJSON_AddNumberToObject(scale, "x", object->transform.scale.x);
    cJSON_AddNumberToObject(scale, "y", object->transform.scale.y);
    cJSON_AddNumberToObject(scale, "z", object->transform.scale.z);

    cJSON_AddStringToObject(geometry_ref, "kind", "shape_asset");
    cJSON_AddStringToObject(geometry_ref, "id", geometry_id);

    if (material_id && material_id[0]) {
        material_ref = cJSON_CreateObject();
        if (!material_ref) return false;
        cJSON_AddItemToObject(object_json, "material_ref", material_ref);
        cJSON_AddStringToObject(material_ref, "id", material_id);
    }

    if (!add_object_tag(tags, "authoring")) return false;
    if (!add_object_tag(tags, "line_drawing")) return false;
    if (tag2 && !add_object_tag(tags, tag2)) return false;
    if (tag3 && !add_object_tag(tags, tag3)) return false;

    cJSON_AddBoolToObject(flags, "visible", object->flags.visible);
    cJSON_AddBoolToObject(flags, "locked", object->flags.locked);
    cJSON_AddBoolToObject(flags, "selectable", object->flags.selectable);
    return true;
}

static cJSON* append_scene_object(cJSON* objects,
                                  const char* object_id,
                                  const char* object_type,
                                  bool is_3d,
                                  Vec3 position,
                                  const char* geometry_id,
                                  const char* material_id,
                                  const char* tag2,
                                  const char* tag3) {
    CoreObject object;
    cJSON* object_json = NULL;

    if (!objects || !object_id || !object_type || !geometry_id) return NULL;

    core_object_init(&object);
    if (core_object_set_identity(&object, object_id, object_type).code != CORE_OK) return NULL;
    if (is_3d) {
        if (core_object_promote_to_full_3d(&object).code != CORE_OK) return NULL;
    } else {
        if (core_object_set_plane_lock(&object, CORE_OBJECT_PLANE_XY).code != CORE_OK) return NULL;
    }
    object.transform.position.x = position.x;
    object.transform.position.y = position.y;
    object.transform.position.z = position.z;
    if (core_object_validate(&object).code != CORE_OK) return NULL;

    object_json = cJSON_CreateObject();
    if (!object_json) return NULL;
    cJSON_AddItemToArray(objects, object_json);

    cJSON_AddStringToObject(object_json, "object_id", object.object_id);
    cJSON_AddStringToObject(object_json, "object_type", object.object_type);
    cJSON_AddStringToObject(object_json, "space_mode_intent", is_3d ? "3d" : "2d");
    cJSON_AddStringToObject(object_json,
                            "dimensional_mode",
                            object.dimensional_mode == CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D
                                ? "full_3d"
                                : "plane_locked");
    if (object.dimensional_mode == CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED) {
        cJSON_AddStringToObject(object_json, "locked_plane", "xy");
        cJSON_AddStringToObject(object_json, "projection_policy", "plane_xy_lock");
        cJSON_AddStringToObject(object_json, "extrusion_policy", "none");
    } else {
        cJSON_AddStringToObject(object_json, "projection_policy", "none");
        cJSON_AddStringToObject(object_json, "extrusion_policy", "none");
    }

    if (!add_object_payload(object_json, &object, geometry_id, material_id, tag2, tag3)) {
        return NULL;
    }
    return object_json;
}

static bool upsert_object_item(cJSON* object, const char* key, cJSON* item) {
    cJSON* existing = NULL;
    if (!object || !key || !item) return false;
    existing = cJSON_GetObjectItemCaseSensitive(object, key);
    if (existing) {
        return cJSON_ReplaceItemInObjectCaseSensitive(object, key, item) != 0;
    }
    cJSON_AddItemToObject(object, key, item);
    return true;
}

static bool upsert_string_item(cJSON* object, const char* key, const char* value) {
    cJSON* item = NULL;
    if (!object || !key || !value) return false;
    item = cJSON_CreateString(value);
    if (!item) return false;
    if (!upsert_object_item(object, key, item)) {
        cJSON_Delete(item);
        return false;
    }
    return true;
}

static bool add_vec3_object_item(cJSON* object, const char* key, Vec3 value) {
    cJSON* vec = NULL;
    if (!object || !key) return false;
    vec = cJSON_CreateObject();
    if (!vec) return false;
    cJSON_AddNumberToObject(vec, "x", value.x);
    cJSON_AddNumberToObject(vec, "y", value.y);
    cJSON_AddNumberToObject(vec, "z", value.z);
    if (!upsert_object_item(object, key, vec)) {
        cJSON_Delete(vec);
        return false;
    }
    return true;
}

static bool refresh_physics_scene_domain_extension(cJSON* root_extensions, const Layout* layout) {
    cJSON* physics_sim = NULL;
    cJSON* scene_domain = NULL;
    if (!root_extensions || !layout) return false;
    if (!layout->scene3d.bounds.enabled ||
        !Layout_SceneBounds3D_IsValid(&layout->scene3d.bounds)) {
        return true;
    }

    physics_sim = duplicate_or_empty_object(cJSON_GetObjectItemCaseSensitive(root_extensions, "physics_sim"));
    if (!physics_sim) return false;
    scene_domain = cJSON_CreateObject();
    if (!scene_domain) {
        cJSON_Delete(physics_sim);
        return false;
    }
    cJSON_AddBoolToObject(scene_domain, "active", true);
    cJSON_AddStringToObject(scene_domain, "shape", "box");
    if (!add_vec3_object_item(scene_domain, "min", layout->scene3d.bounds.min) ||
        !add_vec3_object_item(scene_domain, "max", layout->scene3d.bounds.max)) {
        cJSON_Delete(scene_domain);
        cJSON_Delete(physics_sim);
        return false;
    }
    cJSON_AddBoolToObject(scene_domain, "seeded_from_retained_bounds", false);
    if (!upsert_object_item(physics_sim, "scene_domain", scene_domain)) {
        cJSON_Delete(scene_domain);
        cJSON_Delete(physics_sim);
        return false;
    }
    if (!upsert_object_item(root_extensions, "physics_sim", physics_sim)) {
        cJSON_Delete(physics_sim);
        return false;
    }
    return true;
}

static const cJSON* find_existing_object_extensions(const cJSON* existing_root) {
    if (!existing_root) return NULL;

    const cJSON* objects = cJSON_GetObjectItemCaseSensitive(existing_root, "objects");
    if (!cJSON_IsArray(objects)) return NULL;

    const cJSON* object = NULL;
    cJSON_ArrayForEach(object, objects) {
        const cJSON* object_id = cJSON_GetObjectItemCaseSensitive(object, "object_id");
        if (cJSON_IsString(object_id) && object_id->valuestring &&
            strcmp(object_id->valuestring, kDefaultObjectId) == 0) {
            const cJSON* ext = cJSON_GetObjectItemCaseSensitive(object, "extensions");
            if (cJSON_IsObject(ext)) return ext;
            return NULL;
        }
    }

    return NULL;
}

static const cJSON* find_existing_object_extensions_by_id(const cJSON* existing_root,
                                                          const char* object_id_value) {
    if (!existing_root || !object_id_value || !object_id_value[0]) return NULL;

    const cJSON* objects = cJSON_GetObjectItemCaseSensitive(existing_root, "objects");
    if (!cJSON_IsArray(objects)) return NULL;

    const cJSON* object = NULL;
    cJSON_ArrayForEach(object, objects) {
        const cJSON* object_id = cJSON_GetObjectItemCaseSensitive(object, "object_id");
        if (cJSON_IsString(object_id) && object_id->valuestring &&
            strcmp(object_id->valuestring, object_id_value) == 0) {
            const cJSON* ext = cJSON_GetObjectItemCaseSensitive(object, "extensions");
            if (cJSON_IsObject(ext)) return ext;
            return NULL;
        }
    }

    return NULL;
}

static bool validate_root_contract(const char* scene_id, bool is_3d, double world_scale) {
    CoreSceneRootContract contract;
    core_scene_root_contract_init(&contract);
    if (core_scene_root_contract_set_scene_id(&contract, scene_id).code != CORE_OK) return false;
    contract.space_mode_intent = is_3d ? CORE_SCENE_SPACE_MODE_3D : CORE_SCENE_SPACE_MODE_2D;
    contract.space_mode_default = contract.space_mode_intent;
    contract.unit_kind = CORE_UNIT_METER;
    contract.world_scale = world_scale;
    return core_scene_root_contract_validate(&contract).code == CORE_OK;
}

static bool append_primitive_scene_objects(cJSON* objects,
                                           cJSON* hierarchy,
                                           cJSON* material_bindings,
                                           const Layout* layout,
                                           const cJSON* existing_root,
                                           const char* material_id) {
    const bool debug_export = getenv("LINE_DRAWING_SCENE_EXPORT_DEBUG") != NULL;
    if (!objects || !hierarchy || !layout) return false;

    for (size_t i = 0; i < layout->objectStore.count; ++i) {
        const Object3D* object = &layout->objectStore.items[i];
        CoreObject export_core_meta;
        char geometry_id[96];
        cJSON* object_json = NULL;
        cJSON* object_extensions = NULL;
        cJSON* hierarchy_item = NULL;

        if (object->isDeleted) continue;
        if (!object->coreMeta.object_id[0]) {
            if (debug_export) {
                fprintf(stderr, "[scene_export] skip object index=%zu id=%u: missing core object_id\n",
                        i,
                        object->objectId);
            }
            continue;
        }
        export_core_meta = object->coreMeta;
        if (object->kind == OBJECT3D_KIND_PLANE) {
            export_core_meta.dimensional_mode = CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED;
            export_core_meta.locked_plane = object->coreMeta.locked_plane;
        }

        snprintf(geometry_id,
                 sizeof(geometry_id),
                 "%s",
                 object->kind == OBJECT3D_KIND_MESH_ASSET_INSTANCE
                     ? object->meshInstance.assetId
                     : object->coreMeta.object_id);
        if (object->kind != OBJECT3D_KIND_MESH_ASSET_INSTANCE) {
            char shape_geometry_id[96];
            snprintf(shape_geometry_id, sizeof(shape_geometry_id), "shape_%s", object->coreMeta.object_id);
            snprintf(geometry_id, sizeof(geometry_id), "%s", shape_geometry_id);
        }
        object_json = LineDrawingCanonicalScene_AppendSceneObjectFromCore(
            objects,
            &export_core_meta,
            object->coreMeta.object_id,
            geometry_id,
            material_id,
            object->kind == OBJECT3D_KIND_MESH_ASSET_INSTANCE ? "asset_instance" : "primitive",
            object->kind == OBJECT3D_KIND_MESH_ASSET_INSTANCE
                ? "mesh_asset"
                : (object->kind == OBJECT3D_KIND_RECT_PRISM ? "rect_prism" : "plane"));
        if (!object_json) {
            if (debug_export) {
                fprintf(stderr,
                        "[scene_export] failed append scene object id=%u core_id=%s kind=%d geometry=%s\n",
                        object->objectId,
                        object->coreMeta.object_id,
                        (int)object->kind,
                        geometry_id);
            }
            return false;
        }
        if (!LineDrawingCanonicalScene_AppendDefaultObjectMaterialBinding(material_bindings,
                                                                          object->coreMeta.object_id,
                                                                          material_id,
                                                                          "default")) {
            return false;
        }

        if (object->kind == OBJECT3D_KIND_MESH_ASSET_INSTANCE) {
            cJSON* geometry_ref = cJSON_GetObjectItemCaseSensitive(object_json, "geometry_ref");
            cJSON* mesh_extensions = NULL;
            cJSON* line_ext = NULL;
            if (!cJSON_IsObject(geometry_ref)) {
                if (debug_export) {
                    fprintf(stderr,
                            "[scene_export] mesh object id=%u has no geometry_ref object\n",
                            object->objectId);
                }
                return false;
            }
            cJSON_ReplaceItemInObjectCaseSensitive(geometry_ref, "kind", cJSON_CreateString("mesh_asset"));
            cJSON_ReplaceItemInObjectCaseSensitive(geometry_ref, "id", cJSON_CreateString(object->meshInstance.assetId));

            object_extensions = duplicate_or_empty_object(
                find_existing_object_extensions_by_id(existing_root, object->coreMeta.object_id));
            if (!object_extensions) {
                if (debug_export) {
                    fprintf(stderr,
                            "[scene_export] mesh object id=%u failed to allocate extensions\n",
                            object->objectId);
                }
                return false;
            }
            cJSON_AddItemToObject(object_json, "extensions", object_extensions);
            line_ext = duplicate_or_empty_object(cJSON_GetObjectItemCaseSensitive(object_extensions, "line_drawing"));
            if (!line_ext) {
                if (debug_export) {
                    fprintf(stderr,
                            "[scene_export] mesh object id=%u failed to allocate line_drawing extension\n",
                            object->objectId);
                }
                return false;
            }
            if (!upsert_object_item(object_extensions, "line_drawing", line_ext)) {
                cJSON_Delete(line_ext);
                if (debug_export) {
                    fprintf(stderr,
                            "[scene_export] mesh object id=%u failed to upsert line_drawing extension\n",
                            object->objectId);
                }
                return false;
            }
            cJSON_AddStringToObject(line_ext, "geometry_source", "mesh_asset_instance");
            cJSON_AddStringToObject(line_ext, "source_lane", "objects3d");
            cJSON_AddNumberToObject(line_ext, "layout_object_id", (double)object->objectId);
            cJSON_AddStringToObject(line_ext, "mesh_asset_id", object->meshInstance.assetId);
            cJSON_AddStringToObject(line_ext, "runtime_mesh_path", object->meshInstance.runtimePath);
            cJSON_AddNumberToObject(line_ext, "runtime_vertex_count", (double)object->meshInstance.vertexCount);
            cJSON_AddNumberToObject(line_ext, "runtime_triangle_count", (double)object->meshInstance.triangleCount);
            mesh_extensions = cJSON_CreateObject();
            if (!mesh_extensions) {
                if (debug_export) {
                    fprintf(stderr,
                            "[scene_export] mesh object id=%u failed to allocate local_bounds extension\n",
                            object->objectId);
                }
                return false;
            }
            cJSON_AddItemToObject(line_ext, "local_bounds", mesh_extensions);
            cJSON_AddItemToObject(mesh_extensions,
                                  "min",
                                  vec3_to_json_object(object->meshInstance.localBoundsMin));
            cJSON_AddItemToObject(mesh_extensions,
                                  "max",
                                  vec3_to_json_object(object->meshInstance.localBoundsMax));
        } else {

            object_extensions = duplicate_or_empty_object(
                find_existing_object_extensions_by_id(existing_root, object->coreMeta.object_id));
            if (!object_extensions) {
                if (debug_export) {
                    fprintf(stderr,
                            "[scene_export] primitive object id=%u failed to allocate extensions\n",
                            object->objectId);
                }
                return false;
            }
            cJSON_AddItemToObject(object_json, "extensions", object_extensions);
            if (!LineDrawingCanonicalScene_AddCanonicalPrimitivePayload(object_json, object)) {
                if (debug_export) {
                    fprintf(stderr,
                            "[scene_export] primitive object id=%u failed canonical primitive payload\n",
                            object->objectId);
                }
                return false;
            }
            if (!LineDrawingCanonicalScene_AddPrimitiveExtensionPayload(object_extensions, object)) {
                if (debug_export) {
                    fprintf(stderr,
                            "[scene_export] primitive object id=%u failed primitive extension payload\n",
                            object->objectId);
                }
                return false;
            }
        }

        hierarchy_item = cJSON_CreateObject();
        if (!hierarchy_item) {
            if (debug_export) {
                fprintf(stderr,
                        "[scene_export] object id=%u failed hierarchy allocation\n",
                        object->objectId);
            }
            return false;
        }
        cJSON_AddItemToArray(hierarchy, hierarchy_item);
        cJSON_AddStringToObject(hierarchy_item, "parent_object_id", kDefaultObjectId);
        cJSON_AddStringToObject(hierarchy_item, "child_object_id", object->coreMeta.object_id);
    }

    return true;
}

static cJSON* load_existing_json_file(const char* path) {
    if (!path || !path[0]) return NULL;

    CoreBuffer file_data = {0};
    CoreResult read_result = core_io_read_all(path, &file_data);
    if (read_result.code != CORE_OK || file_data.size == 0 || !file_data.data) {
        return NULL;
    }

    char* text = (char*)malloc(file_data.size + 1u);
    if (!text) {
        core_io_buffer_free(&file_data);
        return NULL;
    }
    memcpy(text, file_data.data, file_data.size);
    text[file_data.size] = '\0';
    core_io_buffer_free(&file_data);

    cJSON* parsed = cJSON_Parse(text);
    free(text);
    return parsed;
}

static cJSON* build_scene_json(const Layout* layout,
                               const char* scene_id,
                               const cJSON* existing_root,
                               const LineDrawingSceneAuthoringOptions* options) {
    bool is_3d;
    bool has_anchors = false;
    size_t active_anchors = 0;
    size_t active_walls = 0;
    size_t active_objects3d = 0;
    Vec3 centroid;
    Vec3 scene_focus;
    cJSON* root = NULL;
    cJSON* objects = NULL;
    cJSON* hierarchy = NULL;
    cJSON* materials = NULL;
    cJSON* material_bindings = NULL;
    cJSON* lights = NULL;
    cJSON* cameras = NULL;
    cJSON* paths = NULL;
    cJSON* constraints = NULL;
    cJSON* root_extensions = NULL;
    cJSON* line_drawing_ext = NULL;
    cJSON* line_drawing_authoring_ext = NULL;
    cJSON* layout_object_json = NULL;
    cJSON* anchor_object_json = NULL;
    cJSON* wall_object_json = NULL;
    cJSON* object_extensions = NULL;
    cJSON* object_line_drawing_ext = NULL;
    const char* resolved_scene_id = NULL;
    const char* resolved_material_id = NULL;
    const char* resolved_material_type = NULL;
    const char* resolved_light_id = NULL;
    const char* resolved_light_type = NULL;
    const char* resolved_camera_id = NULL;
    const char* resolved_camera_type = NULL;
    const char* resolved_camera_path_id = NULL;
    const char* resolved_light_path_id = NULL;
    const char* resolved_preview_mode = NULL;
    const char* resolved_unit_system = NULL;
    const char* resolved_conversion_policy = NULL;
    bool has_camera_path = false;
    bool has_light_path = false;
    bool use_live_scene_authoring = false;
    double resolved_world_scale = kDefaultWorldScale;
    SceneBounds3D framing_bounds = {0};

    if (!layout) return NULL;

    is_3d = layout_uses_3d(layout);
    active_anchors = count_active_anchors(layout);
    active_walls = count_active_walls(layout);
    active_objects3d = count_live_object3d(layout);

    centroid = Layout_ComputeCentroid(layout, &has_anchors);
    if (has_anchors) {
        /* centroid already populated */
    } else {
        centroid.x = 0.0f;
        centroid.y = 0.0f;
        centroid.z = 0.0f;
    }

    scene_focus = centroid;
    if (LineDrawingCanonicalScene_ComputeFramingBounds(layout, &framing_bounds) &&
        Layout_SceneBounds3D_IsValid(&framing_bounds)) {
        scene_focus = scene_bounds_center(framing_bounds);
    }

    root = cJSON_CreateObject();
    if (!root) return NULL;

    resolved_scene_id = get_existing_scene_id(existing_root);
    if (!resolved_scene_id) {
        resolved_scene_id = string_or_default(scene_id, kDefaultSceneId);
    }
    use_live_scene_authoring =
        LineDrawingCanonicalScene_HasLiveSceneAuthoringRecords(layout) &&
        !scene_authoring_options_override_live_records(options);
    resolved_material_id = use_live_scene_authoring && layout->sceneAuthoring.material_count > 0u
        ? string_or_default(layout->sceneAuthoring.materials[0].material_id, kDefaultMaterialId)
        : string_or_default(options ? options->material_id : NULL, kDefaultMaterialId);
    resolved_material_type = string_or_default(options ? options->material_type : NULL, kDefaultMaterialType);
    resolved_light_id = string_or_default(options ? options->light_id : NULL, kDefaultLightId);
    resolved_light_type = string_or_default(options ? options->light_type : NULL, kDefaultLightType);
    resolved_camera_id = string_or_default(options ? options->camera_id : NULL, kDefaultCameraId);
    resolved_camera_type = string_or_default(options ? options->camera_type : NULL,
                                             is_3d ? "perspective" : "orthographic");
    resolved_camera_path_id = options ? options->camera_path_id : NULL;
    resolved_light_path_id = options ? options->light_path_id : NULL;
    has_camera_path = resolved_camera_path_id && resolved_camera_path_id[0];
    has_light_path = resolved_light_path_id && resolved_light_path_id[0];
    resolved_preview_mode = string_or_default(options ? options->preview_mode : NULL,
                                              kDefaultPreviewMode);
    resolved_unit_system =
        string_or_default(options ? options->unit_system : NULL, kDefaultUnitSystem);
    resolved_conversion_policy =
        string_or_default(options ? options->conversion_policy : NULL, kConversionPolicy);
    resolved_world_scale = scene_authoring_world_scale_or_default(options);

    if (core_units_validate_world_scale(resolved_world_scale).code != CORE_OK) return NULL;

    if (!validate_root_contract(resolved_scene_id, is_3d, resolved_world_scale)) return NULL;

    cJSON_AddStringToObject(root, "schema_family", kSchemaFamily);
    cJSON_AddStringToObject(root, "schema_variant", kSchemaVariant);
    cJSON_AddNumberToObject(root, "schema_version", kSchemaVersion);
    cJSON_AddStringToObject(root, "scene_id", resolved_scene_id);
    cJSON_AddStringToObject(root, "space_mode_intent", is_3d ? "3d" : "2d");
    cJSON_AddStringToObject(root, "space_mode_default", is_3d ? "3d" : "2d");
    cJSON_AddStringToObject(root, "conversion_policy", resolved_conversion_policy);
    cJSON_AddStringToObject(root, "unit_system", resolved_unit_system);
    cJSON_AddNumberToObject(root, "world_scale", resolved_world_scale);

    objects = cJSON_CreateArray();
    hierarchy = cJSON_CreateArray();
    materials = cJSON_CreateArray();
    material_bindings = cJSON_CreateArray();
    lights = cJSON_CreateArray();
    cameras = cJSON_CreateArray();
    paths = cJSON_CreateArray();
    constraints = cJSON_CreateArray();
    if (!objects || !hierarchy || !materials || !material_bindings ||
        !lights || !cameras || !paths || !constraints) {
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(root, "objects", objects);
    cJSON_AddItemToObject(root, "hierarchy", hierarchy);
    cJSON_AddItemToObject(root, "materials", materials);
    cJSON_AddItemToObject(root, "material_bindings", material_bindings);
    cJSON_AddItemToObject(root, "lights", lights);
    cJSON_AddItemToObject(root, "cameras", cameras);
    cJSON_AddItemToObject(root, "paths", paths);
    cJSON_AddItemToObject(root, "constraints", constraints);

    root_extensions = duplicate_or_empty_object(
        existing_root ? cJSON_GetObjectItemCaseSensitive(existing_root, "extensions") : NULL);
    if (!root_extensions) {
        cJSON_Delete(root);
        return NULL;
    }
    if (!refresh_physics_scene_domain_extension(root_extensions, layout)) {
        cJSON_Delete(root_extensions);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(root, "extensions", root_extensions);

    layout_object_json = append_scene_object(objects,
                                             kDefaultObjectId,
                                             "curve_path",
                                             is_3d,
                                             centroid,
                                             kDefaultGeometryId,
                                             resolved_material_id,
                                             "layout",
                                             NULL);
    if (!layout_object_json) {
        cJSON_Delete(root);
        return NULL;
    }

    anchor_object_json = append_scene_object(objects,
                                             kAnchorSetObjectId,
                                             "point_set",
                                             is_3d,
                                             centroid,
                                             kAnchorSetGeometryId,
                                             resolved_material_id,
                                             "anchors",
                                             NULL);
    wall_object_json = append_scene_object(objects,
                                           kWallSetObjectId,
                                           "edge_set",
                                           is_3d,
                                           centroid,
                                           kWallSetGeometryId,
                                           resolved_material_id,
                                           "walls",
                                           NULL);
    if (!anchor_object_json || !wall_object_json) {
        cJSON_Delete(root);
        return NULL;
    }
    if (!LineDrawingCanonicalScene_AppendDefaultObjectMaterialBinding(material_bindings,
                                                                      kDefaultObjectId,
                                                                      resolved_material_id,
                                                                      "default") ||
        !LineDrawingCanonicalScene_AppendDefaultObjectMaterialBinding(material_bindings,
                                                                      kAnchorSetObjectId,
                                                                      resolved_material_id,
                                                                      "default") ||
        !LineDrawingCanonicalScene_AppendDefaultObjectMaterialBinding(material_bindings,
                                                                      kWallSetObjectId,
                                                                      resolved_material_id,
                                                                      "default")) {
        cJSON_Delete(root);
        return NULL;
    }

    object_extensions = duplicate_or_empty_object(find_existing_object_extensions(existing_root));
    if (!object_extensions) {
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(layout_object_json, "extensions", object_extensions);

    {
        cJSON* anchor_extensions = cJSON_CreateObject();
        cJSON* wall_extensions = cJSON_CreateObject();
        if (!anchor_extensions || !wall_extensions) {
            cJSON_Delete(root);
            return NULL;
        }
        cJSON_AddItemToObject(anchor_object_json, "extensions", anchor_extensions);
        cJSON_AddItemToObject(wall_object_json, "extensions", wall_extensions);
    }
    {
        cJSON* anchor_extensions = cJSON_GetObjectItemCaseSensitive(anchor_object_json, "extensions");
        cJSON* wall_extensions = cJSON_GetObjectItemCaseSensitive(wall_object_json, "extensions");
        cJSON* anchor_line_ext = cJSON_CreateObject();
        cJSON* wall_line_ext = cJSON_CreateObject();
        if (!anchor_extensions || !wall_extensions || !anchor_line_ext || !wall_line_ext) {
            cJSON_Delete(root);
            return NULL;
        }
        cJSON_AddItemToObject(anchor_extensions, "line_drawing", anchor_line_ext);
        cJSON_AddStringToObject(anchor_line_ext, "source_lane", "anchors");
        cJSON_AddNumberToObject(anchor_line_ext, "active_anchor_count", (double)active_anchors);

        cJSON_AddItemToObject(wall_extensions, "line_drawing", wall_line_ext);
        cJSON_AddStringToObject(wall_line_ext, "source_lane", "walls");
        cJSON_AddNumberToObject(wall_line_ext, "active_wall_count", (double)active_walls);
    }

    line_drawing_ext =
        duplicate_or_empty_object(cJSON_GetObjectItemCaseSensitive(root_extensions, "line_drawing"));
    if (!line_drawing_ext) {
        cJSON_Delete(root);
        return NULL;
    }
    if (!upsert_object_item(root_extensions, "line_drawing", line_drawing_ext)) {
        cJSON_Delete(line_drawing_ext);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddNumberToObject(line_drawing_ext, "active_anchor_count", (double)active_anchors);
    cJSON_AddNumberToObject(line_drawing_ext, "active_wall_count", (double)active_walls);
    cJSON_AddStringToObject(line_drawing_ext, "producer", "line_drawing");
    cJSON_AddStringToObject(line_drawing_ext, "authoring_contract", "np2");
    cJSON_AddNumberToObject(line_drawing_ext, "active_object3d_count", (double)active_objects3d);
    line_drawing_authoring_ext =
        duplicate_or_empty_object(cJSON_GetObjectItemCaseSensitive(line_drawing_ext, "authoring"));
    if (!line_drawing_authoring_ext) {
        cJSON_Delete(root);
        return NULL;
    }
    if (!upsert_object_item(line_drawing_ext, "authoring", line_drawing_authoring_ext)) {
        cJSON_Delete(line_drawing_authoring_ext);
        cJSON_Delete(root);
        return NULL;
    }
    if (!upsert_string_item(line_drawing_authoring_ext, "preview_mode", resolved_preview_mode)) {
        cJSON_Delete(root);
        return NULL;
    }
    if (!add_layout_snapshot_extension(line_drawing_ext, layout)) {
        cJSON_Delete(root);
        return NULL;
    }
    if (!LineDrawingCanonicalScene_PopulateScene3DExtension(line_drawing_ext, layout)) {
        cJSON_Delete(root);
        return NULL;
    }

    object_line_drawing_ext =
        duplicate_or_empty_object(cJSON_GetObjectItemCaseSensitive(object_extensions, "line_drawing"));
    if (!object_line_drawing_ext) {
        cJSON_Delete(root);
        return NULL;
    }
    if (!upsert_object_item(object_extensions, "line_drawing", object_line_drawing_ext)) {
        cJSON_Delete(object_line_drawing_ext);
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddStringToObject(object_line_drawing_ext, "geometry_source", "layout");
    cJSON_AddBoolToObject(object_line_drawing_ext, "is_layout_authoring_object", true);

    if (use_live_scene_authoring) {
        if (!LineDrawingCanonicalScene_AppendLiveSceneAuthoringRecords(materials,
                                                                       lights,
                                                                       cameras,
                                                                       paths,
                                                                       layout,
                                                                       resolved_material_type,
                                                                       resolved_camera_type)) {
            cJSON_Delete(root);
            return NULL;
        }
    } else {
        cJSON* material = cJSON_CreateObject();
        cJSON* material_extensions = cJSON_CreateObject();
        cJSON* material_line_drawing_ext = cJSON_CreateObject();
        if (!material || !material_extensions || !material_line_drawing_ext) {
            cJSON_Delete(root);
            return NULL;
        }
        cJSON_AddItemToArray(materials, material);
        cJSON_AddStringToObject(material, "material_id", resolved_material_id);
        cJSON_AddStringToObject(material, "material_type", resolved_material_type);
        cJSON_AddItemToObject(material, "extensions", material_extensions);
        cJSON_AddItemToObject(material_extensions, "line_drawing", material_line_drawing_ext);
        cJSON_AddStringToObject(material_line_drawing_ext, "preset", "default");
    }

    if (!use_live_scene_authoring) {
        cJSON* light = cJSON_CreateObject();
        cJSON* light_transform = cJSON_CreateObject();
        cJSON* light_position = cJSON_CreateObject();
        cJSON* light_direction = cJSON_CreateObject();
        cJSON* light_aim_target = cJSON_CreateObject();
        cJSON* light_color = cJSON_CreateObject();
        cJSON* light_area_size = cJSON_CreateObject();
        cJSON* light_cone = cJSON_CreateObject();
        float scene_focus_x = scene_focus.x;
        float scene_focus_y = scene_focus.y;
        float scene_focus_z = scene_focus.z;
        float light_offset_x = 3.0f;
        float light_offset_y = 4.0f;
        float light_offset_z = is_3d ? 5.0f : 2.0f;
        if (!light || !light_transform || !light_position || !light_direction ||
            !light_aim_target || !light_color || !light_area_size || !light_cone) {
            cJSON_Delete(root);
            return NULL;
        }
        cJSON_AddItemToArray(lights, light);
        cJSON_AddStringToObject(light, "light_id", resolved_light_id);
        cJSON_AddStringToObject(light, "light_type", resolved_light_type);
        cJSON_AddBoolToObject(light, "enabled", true);
        cJSON_AddStringToObject(light, "position_mode", "independent");
        if (has_light_path) {
            cJSON_AddStringToObject(light, "path_id", resolved_light_path_id);
        }
        cJSON_AddItemToObject(light, "transform", light_transform);
        cJSON_AddItemToObject(light_transform, "position", light_position);
        cJSON_AddNumberToObject(light_position, "x", scene_focus_x + light_offset_x);
        cJSON_AddNumberToObject(light_position, "y", scene_focus_y + light_offset_y);
        cJSON_AddNumberToObject(light_position, "z", scene_focus_z + light_offset_z);
        cJSON_AddItemToObject(light, "direction", light_direction);
        cJSON_AddNumberToObject(light_direction, "x", 0.0);
        cJSON_AddNumberToObject(light_direction, "y", 0.0);
        cJSON_AddNumberToObject(light_direction, "z", -1.0);
        cJSON_AddItemToObject(light, "aim_target", light_aim_target);
        cJSON_AddNumberToObject(light_aim_target, "x", scene_focus_x + light_offset_x);
        cJSON_AddNumberToObject(light_aim_target, "y", scene_focus_y + light_offset_y);
        cJSON_AddNumberToObject(light_aim_target, "z", scene_focus_z + light_offset_z - 4.0f);
        cJSON_AddItemToObject(light, "color", light_color);
        cJSON_AddNumberToObject(light_color, "x", 1.0);
        cJSON_AddNumberToObject(light_color, "y", 1.0);
        cJSON_AddNumberToObject(light_color, "z", 1.0);
        cJSON_AddNumberToObject(light, "intensity", 1.0);
        cJSON_AddNumberToObject(light, "radius", 0.25);
        cJSON_AddItemToObject(light, "area_size", light_area_size);
        cJSON_AddNumberToObject(light_area_size, "width", 2.0);
        cJSON_AddNumberToObject(light_area_size, "height", 2.0);
        cJSON_AddItemToObject(light, "cone", light_cone);
        cJSON_AddNumberToObject(light_cone, "inner_degrees", 25.0);
        cJSON_AddNumberToObject(light_cone, "outer_degrees", 40.0);
        cJSON_AddStringToObject(light, "falloff", "inverse_square");
    }

    if (!use_live_scene_authoring) {
        cJSON* camera = cJSON_CreateObject();
        cJSON* camera_transform = cJSON_CreateObject();
        cJSON* camera_position = cJSON_CreateObject();
        cJSON* camera_forward = cJSON_CreateObject();
        cJSON* camera_up = cJSON_CreateObject();
        cJSON* camera_orientation = cJSON_CreateObject();
        cJSON* camera_look_at = cJSON_CreateObject();
        cJSON* camera_fixed_forward = cJSON_CreateObject();
        float scene_focus_x = scene_focus.x;
        float scene_focus_y = scene_focus.y;
        float scene_focus_z = scene_focus.z;
        float camera_offset_z = is_3d ? 8.0f : 3.0f;
        if (!camera || !camera_transform || !camera_position || !camera_forward ||
            !camera_up || !camera_orientation || !camera_look_at ||
            !camera_fixed_forward) {
            cJSON_Delete(camera);
            cJSON_Delete(camera_transform);
            cJSON_Delete(camera_position);
            cJSON_Delete(camera_forward);
            cJSON_Delete(camera_up);
            cJSON_Delete(camera_orientation);
            cJSON_Delete(camera_look_at);
            cJSON_Delete(camera_fixed_forward);
            cJSON_Delete(root);
            return NULL;
        }
        cJSON_AddItemToArray(cameras, camera);
        cJSON_AddStringToObject(camera, "camera_id", resolved_camera_id);
        cJSON_AddStringToObject(camera, "camera_type", resolved_camera_type);
        if (has_camera_path) {
            cJSON_AddStringToObject(camera, "path_id", resolved_camera_path_id);
        }
        cJSON_AddItemToObject(camera, "transform", camera_transform);
        cJSON_AddItemToObject(camera_transform, "position", camera_position);
        cJSON_AddItemToObject(camera_transform, "forward", camera_forward);
        cJSON_AddItemToObject(camera_transform, "up", camera_up);
        cJSON_AddNumberToObject(camera_position, "x", scene_focus_x);
        cJSON_AddNumberToObject(camera_position, "y", scene_focus_y);
        cJSON_AddNumberToObject(camera_position, "z", scene_focus_z + camera_offset_z);
        cJSON_AddNumberToObject(camera_forward, "x", 0.0f);
        cJSON_AddNumberToObject(camera_forward, "y", 0.0f);
        cJSON_AddNumberToObject(camera_forward, "z", -1.0f);
        cJSON_AddNumberToObject(camera_up, "x", 0.0f);
        cJSON_AddNumberToObject(camera_up, "y", 1.0f);
        cJSON_AddNumberToObject(camera_up, "z", 0.0f);
        cJSON_AddItemToObject(camera, "orientation", camera_orientation);
        cJSON_AddStringToObject(camera_orientation, "mode", "path_facing");
        cJSON_AddNumberToObject(camera_orientation, "roll_degrees", 0.0f);
        cJSON_AddItemToObject(camera_orientation, "look_at_target", camera_look_at);
        cJSON_AddNumberToObject(camera_look_at, "x", scene_focus_x);
        cJSON_AddNumberToObject(camera_look_at, "y", scene_focus_y);
        cJSON_AddNumberToObject(camera_look_at, "z", scene_focus_z);
        cJSON_AddItemToObject(camera_orientation, "fixed_forward", camera_fixed_forward);
        cJSON_AddNumberToObject(camera_fixed_forward, "x", 0.0f);
        cJSON_AddNumberToObject(camera_fixed_forward, "y", 0.0f);
        cJSON_AddNumberToObject(camera_fixed_forward, "z", -1.0f);
        cJSON_AddNumberToObject(camera, "vertical_fov_degrees", 50.0f);
        cJSON_AddNumberToObject(camera, "near_clip", 0.1f);
        cJSON_AddNumberToObject(camera, "far_clip", 250.0f);
    }

    if (!use_live_scene_authoring && has_camera_path) {
        const float camera_z = scene_focus.z + (is_3d ? 8.0f : 3.0f);
        if (!append_scene_path(paths,
                               resolved_camera_path_id,
                               "camera",
                               string_or_default(options ? options->camera_path_label : NULL,
                                                 kDefaultCameraPathLabel),
                               "camera_id",
                               resolved_camera_id,
                               (Vec3){ scene_focus.x - 2.0f, scene_focus.y - 5.0f, camera_z },
                               (Vec3){ scene_focus.x, scene_focus.y - 3.0f, camera_z - 1.0f },
                               (Vec3){ scene_focus.x + 2.0f, scene_focus.y - 5.0f, camera_z })) {
            cJSON_Delete(root);
            return NULL;
        }
    }

    if (!use_live_scene_authoring && has_light_path) {
        const float light_z = scene_focus.z + (is_3d ? 5.0f : 2.0f);
        if (!append_scene_path(paths,
                               resolved_light_path_id,
                               "light",
                               string_or_default(options ? options->light_path_label : NULL,
                                                 kDefaultLightPathLabel),
                               "light_id",
                               resolved_light_id,
                               (Vec3){ scene_focus.x + 1.0f, scene_focus.y + 2.0f, light_z },
                               (Vec3){ scene_focus.x + 3.0f, scene_focus.y + 4.0f, light_z + 1.0f },
                               (Vec3){ scene_focus.x + 5.0f, scene_focus.y + 2.0f, light_z })) {
            cJSON_Delete(root);
            return NULL;
        }
    }

    {
        cJSON* hierarchy_anchor = cJSON_CreateObject();
        cJSON* hierarchy_wall = cJSON_CreateObject();
        if (!hierarchy_anchor || !hierarchy_wall) {
            cJSON_Delete(root);
            return NULL;
        }
        cJSON_AddItemToArray(hierarchy, hierarchy_anchor);
        cJSON_AddStringToObject(hierarchy_anchor, "parent_object_id", kDefaultObjectId);
        cJSON_AddStringToObject(hierarchy_anchor, "child_object_id", kAnchorSetObjectId);
        cJSON_AddItemToArray(hierarchy, hierarchy_wall);
        cJSON_AddStringToObject(hierarchy_wall, "parent_object_id", kDefaultObjectId);
        cJSON_AddStringToObject(hierarchy_wall, "child_object_id", kWallSetObjectId);
    }

    if (!append_primitive_scene_objects(objects,
                                        hierarchy,
                                        material_bindings,
                                        layout,
                                        existing_root,
                                        resolved_material_id)) {
        cJSON_Delete(root);
        return NULL;
    }

    if (!LineDrawingCanonicalScene_AppendMaterialBindingOptions(
            material_bindings,
            options ? options->material_bindings : NULL,
            options ? options->material_binding_count : 0u,
            resolved_material_id)) {
        cJSON_Delete(root);
        return NULL;
    }

    return root;
}

static bool validate_scene_json_for_persistence(const cJSON* root) {
    const cJSON* objects = NULL;
    const cJSON* materials = NULL;
    const cJSON* material_bindings = NULL;
    const cJSON* lights = NULL;
    const cJSON* cameras = NULL;

    if (!root || !cJSON_IsObject(root)) return false;
    if (!cJSON_IsString(cJSON_GetObjectItemCaseSensitive(root, "schema_variant"))) return false;
    if (!cJSON_IsString(cJSON_GetObjectItemCaseSensitive(root, "scene_id"))) return false;
    if (!cJSON_IsString(cJSON_GetObjectItemCaseSensitive(root, "space_mode_intent"))) return false;
    if (!cJSON_IsString(cJSON_GetObjectItemCaseSensitive(root, "space_mode_default"))) return false;
    if (!cJSON_IsString(cJSON_GetObjectItemCaseSensitive(root, "conversion_policy"))) return false;

    objects = cJSON_GetObjectItemCaseSensitive(root, "objects");
    materials = cJSON_GetObjectItemCaseSensitive(root, "materials");
    material_bindings = cJSON_GetObjectItemCaseSensitive(root, "material_bindings");
    lights = cJSON_GetObjectItemCaseSensitive(root, "lights");
    cameras = cJSON_GetObjectItemCaseSensitive(root, "cameras");
    if (!cJSON_IsArray(objects) || cJSON_GetArraySize(objects) <= 0) return false;
    if (!cJSON_IsArray(materials) || cJSON_GetArraySize(materials) <= 0) return false;
    if (!cJSON_IsArray(material_bindings)) return false;
    if (!cJSON_IsArray(lights) || cJSON_GetArraySize(lights) <= 0) return false;
    if (!cJSON_IsArray(cameras) || cJSON_GetArraySize(cameras) <= 0) return false;
    return true;
}

char* LineDrawingCanonicalScene_ExportLayoutToStringWithOptions(
    const Layout* layout,
    const char* sceneId,
    const LineDrawingSceneAuthoringOptions* options) {
    cJSON* root = NULL;
    char* out = NULL;
    if (!validate_options(options)) return NULL;
    root = build_scene_json(layout, sceneId, NULL, options);
    if (!root) return NULL;
    if (!validate_scene_json_for_persistence(root)) {
        cJSON_Delete(root);
        return NULL;
    }
    out = cJSON_PrintBuffered(root, 1024, cJSON_True);
    cJSON_Delete(root);
    return out;
}

char* LineDrawingCanonicalScene_ExportLayoutToString(const Layout* layout,
                                                     const char* sceneId) {
    return LineDrawingCanonicalScene_ExportLayoutToStringWithOptions(layout, sceneId, NULL);
}

bool LineDrawingCanonicalScene_ExportLayoutToFileWithOptions(
    const Layout* layout,
    const char* sceneId,
    const char* outputPath,
    const LineDrawingSceneAuthoringOptions* options) {
    cJSON* existing_root = NULL;
    cJSON* root = NULL;
    char* out = NULL;
    CoreResult write_result;

    if (!layout || !outputPath || !outputPath[0]) return false;
    if (!validate_options(options)) return false;

    existing_root = load_existing_json_file(outputPath);
    root = build_scene_json(layout, sceneId, existing_root, options);
    cJSON_Delete(existing_root);
    if (!root) return false;
    if (!validate_scene_json_for_persistence(root)) {
        cJSON_Delete(root);
        return false;
    }

    out = cJSON_PrintBuffered(root, 1024, cJSON_True);
    cJSON_Delete(root);
    if (!out) return false;

    write_result = core_io_write_all(outputPath, out, strlen(out));
    free(out);
    return write_result.code == CORE_OK;
}

bool LineDrawingCanonicalScene_ExportLayoutToFile(const Layout* layout,
                                                  const char* sceneId,
                                                  const char* outputPath) {
    return LineDrawingCanonicalScene_ExportLayoutToFileWithOptions(layout, sceneId, outputPath, NULL);
}
