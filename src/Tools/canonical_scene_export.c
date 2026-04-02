#include "Tools/canonical_scene_export.h"

#include "core_io.h"
#include "core_object.h"
#include "core_units.h"
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
static const char* kDefaultMaterialType = "flat_color";
static const char* kDefaultLightType = "directional";
static const char* kDefaultUnitSystem = "meters";
static const char* kConversionPolicy = "explicit_only";

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

static bool validate_options(const LineDrawingSceneAuthoringOptions* options) {
    if (!options) return true;

    if (options->material_id && !is_valid_token(options->material_id)) return false;
    if (options->light_id && !is_valid_token(options->light_id)) return false;
    if (options->camera_id && !is_valid_token(options->camera_id)) return false;

    if (options->material_type && !is_allowed_material_type(options->material_type)) return false;
    if (options->light_type && !is_allowed_light_type(options->light_type)) return false;
    if (options->camera_type && !is_allowed_camera_type(options->camera_type)) return false;
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

static bool layout_uses_3d(const Layout* layout) {
    if (!layout) return false;
    for (size_t i = 0; i < layout->anchorCount; ++i) {
        const Anchor* anchor = &layout->anchors[i];
        if (anchor->isDeleted) continue;
        if (fabs(anchor->pos.z) > 0.0001f) return true;
    }
    return false;
}

static cJSON* duplicate_or_empty_object(const cJSON* source) {
    if (!source || !cJSON_IsObject(source)) {
        return cJSON_CreateObject();
    }
    return cJSON_Duplicate(source, 1);
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
    Vec3 centroid;
    cJSON* root = NULL;
    cJSON* objects = NULL;
    cJSON* hierarchy = NULL;
    cJSON* materials = NULL;
    cJSON* lights = NULL;
    cJSON* cameras = NULL;
    cJSON* constraints = NULL;
    cJSON* root_extensions = NULL;
    cJSON* line_drawing_ext = NULL;
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

    if (!layout) return NULL;

    if (core_units_validate_world_scale(1.0).code != CORE_OK) return NULL;

    is_3d = layout_uses_3d(layout);
    active_anchors = count_active_anchors(layout);
    active_walls = count_active_walls(layout);

    centroid = Layout_ComputeCentroid(layout, &has_anchors);
    if (has_anchors) {
        /* centroid already populated */
    } else {
        centroid.x = 0.0f;
        centroid.y = 0.0f;
        centroid.z = 0.0f;
    }

    root = cJSON_CreateObject();
    if (!root) return NULL;

    resolved_scene_id = get_existing_scene_id(existing_root);
    if (!resolved_scene_id) {
        resolved_scene_id = string_or_default(scene_id, kDefaultSceneId);
    }
    resolved_material_id = string_or_default(options ? options->material_id : NULL, kDefaultMaterialId);
    resolved_material_type = string_or_default(options ? options->material_type : NULL, kDefaultMaterialType);
    resolved_light_id = string_or_default(options ? options->light_id : NULL, kDefaultLightId);
    resolved_light_type = string_or_default(options ? options->light_type : NULL, kDefaultLightType);
    resolved_camera_id = string_or_default(options ? options->camera_id : NULL, kDefaultCameraId);
    resolved_camera_type = string_or_default(options ? options->camera_type : NULL,
                                             is_3d ? "perspective" : "orthographic");

    cJSON_AddStringToObject(root, "schema_family", kSchemaFamily);
    cJSON_AddStringToObject(root, "schema_variant", kSchemaVariant);
    cJSON_AddNumberToObject(root, "schema_version", kSchemaVersion);
    cJSON_AddStringToObject(root, "scene_id", resolved_scene_id);
    cJSON_AddStringToObject(root, "space_mode_intent", is_3d ? "3d" : "2d");
    cJSON_AddStringToObject(root, "space_mode_default", is_3d ? "3d" : "2d");
    cJSON_AddStringToObject(root, "conversion_policy", kConversionPolicy);
    cJSON_AddStringToObject(root, "unit_system", kDefaultUnitSystem);
    cJSON_AddNumberToObject(root, "world_scale", 1.0);

    objects = cJSON_CreateArray();
    hierarchy = cJSON_CreateArray();
    materials = cJSON_CreateArray();
    lights = cJSON_CreateArray();
    cameras = cJSON_CreateArray();
    constraints = cJSON_CreateArray();
    if (!objects || !hierarchy || !materials || !lights || !cameras || !constraints) {
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(root, "objects", objects);
    cJSON_AddItemToObject(root, "hierarchy", hierarchy);
    cJSON_AddItemToObject(root, "materials", materials);
    cJSON_AddItemToObject(root, "lights", lights);
    cJSON_AddItemToObject(root, "cameras", cameras);
    cJSON_AddItemToObject(root, "constraints", constraints);

    root_extensions = duplicate_or_empty_object(
        existing_root ? cJSON_GetObjectItemCaseSensitive(existing_root, "extensions") : NULL);
    if (!root_extensions) {
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

    {
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

    {
        cJSON* light = cJSON_CreateObject();
        cJSON* light_transform = cJSON_CreateObject();
        cJSON* light_position = cJSON_CreateObject();
        if (!light || !light_transform || !light_position) {
            cJSON_Delete(root);
            return NULL;
        }
        cJSON_AddItemToArray(lights, light);
        cJSON_AddStringToObject(light, "light_id", resolved_light_id);
        cJSON_AddStringToObject(light, "light_type", resolved_light_type);
        cJSON_AddItemToObject(light, "transform", light_transform);
        cJSON_AddItemToObject(light_transform, "position", light_position);
        cJSON_AddNumberToObject(light_position, "x", centroid.x + 3.0f);
        cJSON_AddNumberToObject(light_position, "y", centroid.y + 4.0f);
        cJSON_AddNumberToObject(light_position, "z", centroid.z + (is_3d ? 5.0f : 2.0f));
    }

    {
        cJSON* camera = cJSON_CreateObject();
        cJSON* camera_transform = cJSON_CreateObject();
        cJSON* camera_position = cJSON_CreateObject();
        if (!camera || !camera_transform || !camera_position) {
            cJSON_Delete(root);
            return NULL;
        }
        cJSON_AddItemToArray(cameras, camera);
        cJSON_AddStringToObject(camera, "camera_id", resolved_camera_id);
        cJSON_AddStringToObject(camera, "camera_type", resolved_camera_type);
        cJSON_AddItemToObject(camera, "transform", camera_transform);
        cJSON_AddItemToObject(camera_transform, "position", camera_position);
        cJSON_AddNumberToObject(camera_position, "x", centroid.x);
        cJSON_AddNumberToObject(camera_position, "y", centroid.y);
        cJSON_AddNumberToObject(camera_position, "z", centroid.z + (is_3d ? 8.0f : 3.0f));
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

    return root;
}

static bool validate_scene_json_for_persistence(const cJSON* root) {
    const cJSON* objects = NULL;
    const cJSON* materials = NULL;
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
    lights = cJSON_GetObjectItemCaseSensitive(root, "lights");
    cameras = cJSON_GetObjectItemCaseSensitive(root, "cameras");
    if (!cJSON_IsArray(objects) || cJSON_GetArraySize(objects) <= 0) return false;
    if (!cJSON_IsArray(materials) || cJSON_GetArraySize(materials) <= 0) return false;
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
