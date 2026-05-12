#include "Tools/scene_import.h"

#include "Layout/layout_json.h"
#include "core_io.h"
#include "core_object.h"
#include "cjson/cJSON.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* k_authoring_schema_variant = "scene_authoring_v1";
static const char* k_runtime_schema_variant = "scene_runtime_v1";

static void write_diagnostics(char* diagnostics, size_t diagnostics_size, const char* message) {
    if (!diagnostics || diagnostics_size == 0u) return;
    if (!message) {
        diagnostics[0] = '\0';
        return;
    }
    snprintf(diagnostics, diagnostics_size, "%s", message);
}

static cJSON* load_root_json(const char* path, char* diagnostics, size_t diagnostics_size) {
    CoreBuffer file_data = {0};
    CoreResult read_result = {0};
    char* text = NULL;
    cJSON* root = NULL;

    read_result = core_io_read_all(path, &file_data);
    if (read_result.code != CORE_OK || !file_data.data || file_data.size == 0u) {
        write_diagnostics(diagnostics, diagnostics_size, "failed to read scene authoring file");
        return NULL;
    }

    text = (char*)malloc(file_data.size + 1u);
    if (!text) {
        core_io_buffer_free(&file_data);
        write_diagnostics(diagnostics, diagnostics_size, "failed to allocate scene import buffer");
        return NULL;
    }
    memcpy(text, file_data.data, file_data.size);
    text[file_data.size] = '\0';
    core_io_buffer_free(&file_data);

    root = cJSON_Parse(text);
    free(text);
    if (!root) {
        write_diagnostics(diagnostics, diagnostics_size, "failed to parse scene authoring JSON");
        return NULL;
    }
    return root;
}

static cJSON* find_layout_snapshot(cJSON* root) {
    cJSON* extensions = NULL;
    cJSON* line_drawing = NULL;
    if (!cJSON_IsObject(root)) return NULL;
    extensions = cJSON_GetObjectItemCaseSensitive(root, "extensions");
    if (!cJSON_IsObject(extensions)) return NULL;
    line_drawing = cJSON_GetObjectItemCaseSensitive(extensions, "line_drawing");
    if (!cJSON_IsObject(line_drawing)) return NULL;
    return cJSON_GetObjectItemCaseSensitive(line_drawing, "layout_snapshot");
}

static cJSON* find_line_drawing_extension(cJSON* root) {
    cJSON* extensions = NULL;
    if (!cJSON_IsObject(root)) return NULL;
    extensions = cJSON_GetObjectItemCaseSensitive(root, "extensions");
    if (!cJSON_IsObject(extensions)) return NULL;
    return cJSON_GetObjectItemCaseSensitive(extensions, "line_drawing");
}

static bool vec3_from_json_object(const cJSON* node, Vec3* out) {
    const cJSON* x = NULL;
    const cJSON* y = NULL;
    const cJSON* z = NULL;
    if (!cJSON_IsObject(node) || !out) return false;
    x = cJSON_GetObjectItemCaseSensitive(node, "x");
    y = cJSON_GetObjectItemCaseSensitive(node, "y");
    z = cJSON_GetObjectItemCaseSensitive(node, "z");
    if (!cJSON_IsNumber(x) || !cJSON_IsNumber(y) || !cJSON_IsNumber(z)) return false;
    out->x = (float)x->valuedouble;
    out->y = (float)y->valuedouble;
    out->z = (float)z->valuedouble;
    return true;
}

static const cJSON* object_item2(const cJSON* node, const char* primary, const char* fallback) {
    const cJSON* item = NULL;
    if (!cJSON_IsObject(node) || !primary) return NULL;
    item = cJSON_GetObjectItemCaseSensitive(node, primary);
    if (!item && fallback) {
        item = cJSON_GetObjectItemCaseSensitive(node, fallback);
    }
    return item;
}

static bool plane_frame_from_json_object(const cJSON* node, PlaneFrame3* out) {
    if (!cJSON_IsObject(node) || !out) return false;
    return vec3_from_json_object(cJSON_GetObjectItemCaseSensitive(node, "origin"), &out->origin) &&
           vec3_from_json_object(object_item2(node, "axisU", "axis_u"), &out->axisU) &&
           vec3_from_json_object(object_item2(node, "axisV", "axis_v"), &out->axisV) &&
           vec3_from_json_object(cJSON_GetObjectItemCaseSensitive(node, "normal"), &out->normal);
}

static bool transform_from_scene_json(const cJSON* node, Transform3D* out) {
    const cJSON* position = NULL;
    const cJSON* rotation = NULL;
    const cJSON* scale = NULL;
    if (!cJSON_IsObject(node) || !out) return false;
    position = cJSON_GetObjectItemCaseSensitive(node, "position");
    rotation = object_item2(node, "rotationDeg", "rotation");
    scale = cJSON_GetObjectItemCaseSensitive(node, "scale");
    if (!vec3_from_json_object(position, &out->position)) return false;
    if (!vec3_from_json_object(rotation, &out->rotationDeg)) return false;
    if (!vec3_from_json_object(scale, &out->scale)) return false;
    return true;
}

static ViewPlaneAxis handle_axis_from_json(const cJSON* node) {
    if (!cJSON_IsString(node) || !node->valuestring) return VIEW_PLANE_XY;
    if (strcmp(node->valuestring, "yz") == 0) return VIEW_PLANE_YZ;
    if (strcmp(node->valuestring, "xz") == 0) return VIEW_PLANE_XZ;
    return VIEW_PLANE_XY;
}

static ConstructionPlaneMode construction_plane_mode_from_json(const cJSON* node) {
    if (!cJSON_IsString(node) || !node->valuestring) return CONSTRUCTION_PLANE_MODE_AXIS_ALIGNED;
    if (strcmp(node->valuestring, "custom_frame") == 0) return CONSTRUCTION_PLANE_MODE_CUSTOM_FRAME;
    return CONSTRUCTION_PLANE_MODE_AXIS_ALIGNED;
}

static CoreObjectDimensionalMode dimensional_mode_from_json(const cJSON* node) {
    if (!cJSON_IsString(node) || !node->valuestring) return CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D;
    if (strcmp(node->valuestring, "plane_locked") == 0) return CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED;
    return CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D;
}

static CoreObjectPlane core_plane_from_json(const cJSON* node) {
    if (!cJSON_IsString(node) || !node->valuestring) return CORE_OBJECT_PLANE_XY;
    if (strcmp(node->valuestring, "yz") == 0) return CORE_OBJECT_PLANE_YZ;
    if (strcmp(node->valuestring, "xz") == 0) return CORE_OBJECT_PLANE_XZ;
    return CORE_OBJECT_PLANE_XY;
}

static uint32_t parse_object_numeric_id(const cJSON* object_node, const cJSON* line_drawing_ext) {
    const cJSON* layout_object_id = NULL;
    const cJSON* object_id = NULL;
    const char* cursor = NULL;
    uint32_t parsed = 0u;
    if (cJSON_IsObject(line_drawing_ext)) {
        layout_object_id = cJSON_GetObjectItemCaseSensitive(line_drawing_ext, "layout_object_id");
        if (cJSON_IsNumber(layout_object_id) && layout_object_id->valuedouble > 0.0) {
            return (uint32_t)layout_object_id->valuedouble;
        }
    }

    object_id = cJSON_GetObjectItemCaseSensitive(object_node, "object_id");
    if (!cJSON_IsString(object_id) || !object_id->valuestring) return 0u;
    cursor = object_id->valuestring;
    while (*cursor && !isdigit((unsigned char)*cursor)) {
        ++cursor;
    }
    if (!*cursor) return 0u;
    while (*cursor && isdigit((unsigned char)*cursor)) {
        parsed = parsed * 10u + (uint32_t)(*cursor - '0');
        ++cursor;
    }
    return parsed;
}

static bool scene_bounds_from_json(const cJSON* node, SceneBounds3D* out) {
    const cJSON* enabled = NULL;
    const cJSON* clamp_on_edit = NULL;
    if (!cJSON_IsObject(node) || !out) return false;
    enabled = cJSON_GetObjectItemCaseSensitive(node, "enabled");
    clamp_on_edit = object_item2(node, "clampOnEdit", "clamp_on_edit");
    if (cJSON_IsBool(enabled)) out->enabled = cJSON_IsTrue(enabled);
    if (cJSON_IsBool(clamp_on_edit)) out->clampOnEdit = cJSON_IsTrue(clamp_on_edit);
    if (!vec3_from_json_object(cJSON_GetObjectItemCaseSensitive(node, "min"), &out->min)) return false;
    if (!vec3_from_json_object(cJSON_GetObjectItemCaseSensitive(node, "max"), &out->max)) return false;
    return Layout_SceneBounds3D_IsValid(out);
}

static bool construction_plane_from_scene_json(const cJSON* node, ConstructionPlane3D* out) {
    const cJSON* offset = NULL;
    const cJSON* custom_frame = NULL;
    if (!cJSON_IsObject(node) || !out) return false;
    out->mode = construction_plane_mode_from_json(cJSON_GetObjectItemCaseSensitive(node, "mode"));
    out->axisAligned.axis = handle_axis_from_json(cJSON_GetObjectItemCaseSensitive(node, "axis"));
    offset = cJSON_GetObjectItemCaseSensitive(node, "offset");
    if (cJSON_IsNumber(offset)) {
        out->axisAligned.offset = (float)offset->valuedouble;
    }
    custom_frame = object_item2(node, "customFrame", "custom_frame");
    if (cJSON_IsObject(custom_frame)) {
        (void)plane_frame_from_json_object(custom_frame, &out->customFrame);
    }
    return Layout_ConstructionPlane3D_IsValid(out);
}

static void apply_legacy_scene3d_settings(Layout* layout, const cJSON* root) {
    cJSON* line_drawing_ext = find_line_drawing_extension((cJSON*)root);
    const cJSON* scene3d = NULL;
    SceneBounds3D bounds = {0};
    ConstructionPlane3D plane = {0};
    if (!layout || !cJSON_IsObject(line_drawing_ext)) return;
    scene3d = cJSON_GetObjectItemCaseSensitive(line_drawing_ext, "scene3d");
    if (!cJSON_IsObject(scene3d)) return;

    bounds = layout->scene3d.bounds;
    if (scene_bounds_from_json(object_item2(scene3d, "authoring_bounds", "authoringBounds"), &bounds) ||
        scene_bounds_from_json(cJSON_GetObjectItemCaseSensitive(scene3d, "bounds"), &bounds)) {
        layout->scene3d.bounds = bounds;
    }

    plane = layout->scene3d.constructionPlane;
    if (construction_plane_from_scene_json(cJSON_GetObjectItemCaseSensitive(scene3d, "construction_plane"), &plane) ||
        construction_plane_from_scene_json(cJSON_GetObjectItemCaseSensitive(scene3d, "constructionPlane"), &plane)) {
        layout->scene3d.constructionPlane = plane;
    }
}

static bool import_legacy_scene_object(Layout* layout,
                                       const cJSON* object_node,
                                       uint32_t* inout_max_id) {
    const cJSON* object_type_node = NULL;
    const cJSON* transform_node = NULL;
    const cJSON* extensions = NULL;
    const cJSON* line_drawing_ext = NULL;
    const cJSON* payload = NULL;
    const cJSON* primitive = NULL;
    const cJSON* width = NULL;
    const cJSON* height = NULL;
    const cJSON* depth = NULL;
    const cJSON* lock_to_construction_plane = NULL;
    const cJSON* lock_to_bounds = NULL;
    const cJSON* frame = NULL;
    const char* object_type = NULL;
    Object3DKind kind = OBJECT3D_KIND_UNKNOWN;
    Transform3D transform = Layout_Transform3D_Default();
    CoreObjectDimensionalMode dimensional_mode = CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D;
    CoreObjectPlane locked_plane = CORE_OBJECT_PLANE_XY;
    uint32_t created_id = 0u;
    uint32_t parsed_id = 0u;
    Object3D* object = NULL;

    if (!layout || !cJSON_IsObject(object_node)) return false;

    object_type_node = cJSON_GetObjectItemCaseSensitive(object_node, "object_type");
    if (!cJSON_IsString(object_type_node) || !object_type_node->valuestring) return false;
    object_type = object_type_node->valuestring;
    if (strcmp(object_type, "plane_primitive") == 0) {
        kind = OBJECT3D_KIND_PLANE;
    } else if (strcmp(object_type, "rect_prism_primitive") == 0) {
        kind = OBJECT3D_KIND_RECT_PRISM;
    } else {
        return false;
    }

    transform_node = cJSON_GetObjectItemCaseSensitive(object_node, "transform");
    if (cJSON_IsObject(transform_node)) {
        (void)transform_from_scene_json(transform_node, &transform);
    }
    dimensional_mode = dimensional_mode_from_json(cJSON_GetObjectItemCaseSensitive(object_node, "dimensional_mode"));
    locked_plane = core_plane_from_json(cJSON_GetObjectItemCaseSensitive(object_node, "locked_plane"));

    created_id = Layout_ObjectStore_Create(&layout->objectStore,
                                           kind,
                                           &transform,
                                           object_type,
                                           dimensional_mode,
                                           locked_plane);
    if (created_id == 0u) return false;
    object = Layout_ObjectStore_Find(&layout->objectStore, created_id);
    if (!object) return false;

    extensions = cJSON_GetObjectItemCaseSensitive(object_node, "extensions");
    line_drawing_ext = cJSON_IsObject(extensions)
        ? cJSON_GetObjectItemCaseSensitive(extensions, "line_drawing")
        : NULL;
    parsed_id = parse_object_numeric_id(object_node, line_drawing_ext);
    if (parsed_id > 0u && parsed_id != created_id) {
        object->objectId = parsed_id;
        {
            char object_id_buf[64];
            snprintf(object_id_buf, sizeof(object_id_buf), "obj3d_%u", parsed_id);
            (void)core_object_set_identity(&object->coreMeta, object_id_buf, object->coreMeta.object_type);
        }
    }
    if (parsed_id > 0u && inout_max_id && parsed_id > *inout_max_id) {
        *inout_max_id = parsed_id;
    } else if (created_id > 0u && inout_max_id && created_id > *inout_max_id) {
        *inout_max_id = created_id;
    }

    payload = cJSON_IsObject(line_drawing_ext)
        ? cJSON_GetObjectItemCaseSensitive(line_drawing_ext, "primitive_payload")
        : NULL;
    primitive = cJSON_GetObjectItemCaseSensitive(object_node, "primitive");
    if (!cJSON_IsObject(payload)) payload = primitive;
    if (!cJSON_IsObject(payload)) {
        (void)Layout_ObjectStore_Delete(&layout->objectStore, object->objectId);
        return false;
    }

    width = cJSON_GetObjectItemCaseSensitive(payload, "width");
    height = cJSON_GetObjectItemCaseSensitive(payload, "height");
    depth = cJSON_GetObjectItemCaseSensitive(payload, "depth");
    lock_to_construction_plane = object_item2(payload, "lock_to_construction_plane", "lockToConstructionPlane");
    lock_to_bounds = object_item2(payload, "lock_to_bounds", "lockToBounds");
    frame = cJSON_GetObjectItemCaseSensitive(payload, "frame");

    if (kind == OBJECT3D_KIND_PLANE) {
        if (cJSON_IsNumber(width) && width->valuedouble > 0.0) object->plane.width = (float)width->valuedouble;
        if (cJSON_IsNumber(height) && height->valuedouble > 0.0) object->plane.height = (float)height->valuedouble;
        if (cJSON_IsBool(lock_to_construction_plane)) object->plane.lockToConstructionPlane = cJSON_IsTrue(lock_to_construction_plane);
        if (cJSON_IsBool(lock_to_bounds)) object->plane.lockToBounds = cJSON_IsTrue(lock_to_bounds);
        if (cJSON_IsObject(frame)) (void)plane_frame_from_json_object(frame, &object->plane.frame);
        object->plane.frame.origin = object->transform.position;
    } else if (kind == OBJECT3D_KIND_RECT_PRISM) {
        if (cJSON_IsNumber(width) && width->valuedouble > 0.0) object->rectPrism.width = (float)width->valuedouble;
        if (cJSON_IsNumber(height) && height->valuedouble > 0.0) object->rectPrism.height = (float)height->valuedouble;
        if (cJSON_IsNumber(depth) && depth->valuedouble >= 0.0) object->rectPrism.depth = (float)depth->valuedouble;
        if (cJSON_IsBool(lock_to_construction_plane)) object->rectPrism.lockToConstructionPlane = cJSON_IsTrue(lock_to_construction_plane);
        if (cJSON_IsBool(lock_to_bounds)) object->rectPrism.lockToBounds = cJSON_IsTrue(lock_to_bounds);
        if (cJSON_IsObject(frame)) (void)plane_frame_from_json_object(frame, &object->rectPrism.frame);
        object->rectPrism.frame.origin = object->transform.position;
    }

    if (!Layout_ObjectStore_ValidateObject(object)) {
        (void)Layout_ObjectStore_Delete(&layout->objectStore, object->objectId);
        return false;
    }
    return true;
}

static bool import_legacy_scene_authoring(Layout* layout,
                                          cJSON* root,
                                          char* diagnostics,
                                          size_t diagnostics_size) {
    Layout temp;
    const cJSON* objects = NULL;
    uint32_t max_id = 0u;
    bool imported_any = false;
    if (!layout || !cJSON_IsObject(root)) return false;

    Layout_Init(&temp, layout->gridSize > 0.0f ? layout->gridSize : 1.0f);
    apply_legacy_scene3d_settings(&temp, root);

    objects = cJSON_GetObjectItemCaseSensitive(root, "objects");
    if (cJSON_IsArray(objects)) {
        const int count = cJSON_GetArraySize(objects);
        for (int i = 0; i < count; ++i) {
            if (import_legacy_scene_object(&temp, cJSON_GetArrayItem(objects, i), &max_id)) {
                imported_any = true;
            }
        }
    }

    if (max_id >= temp.objectStore.nextObjectId) {
        temp.objectStore.nextObjectId = max_id + 1u;
    }

    if (!imported_any) {
        Layout_Free(&temp);
        write_diagnostics(diagnostics,
                          diagnostics_size,
                          "scene is missing layout_snapshot and legacy fallback found no supported line_drawing primitives");
        return false;
    }

    Layout_Free(layout);
    *layout = temp;
    write_diagnostics(diagnostics, diagnostics_size, NULL);
    return true;
}

bool LineDrawingSceneImport_LoadLayoutFromAuthoringFile(Layout* layout,
                                                        const char* authoring_path,
                                                        char* diagnostics,
                                                        size_t diagnostics_size) {
    cJSON* root = NULL;
    cJSON* schema_variant = NULL;
    cJSON* layout_snapshot = NULL;
    char* snapshot_text = NULL;
    bool ok = false;

    write_diagnostics(diagnostics, diagnostics_size, NULL);
    if (!layout || !authoring_path || !authoring_path[0]) {
        write_diagnostics(diagnostics, diagnostics_size, "scene import requires a target layout and path");
        return false;
    }

    root = load_root_json(authoring_path, diagnostics, diagnostics_size);
    if (!root) return false;

    schema_variant = cJSON_GetObjectItemCaseSensitive(root, "schema_variant");
    if (!cJSON_IsString(schema_variant) || !schema_variant->valuestring) {
        write_diagnostics(diagnostics, diagnostics_size, "scene import requires a scene_authoring_v1 file");
        goto cleanup;
    }
    if (strcmp(schema_variant->valuestring, k_runtime_schema_variant) == 0) {
        write_diagnostics(diagnostics, diagnostics_size, "scene_runtime.json is compiled output; load scene_authoring.json instead");
        goto cleanup;
    }
    if (strcmp(schema_variant->valuestring, k_authoring_schema_variant) != 0) {
        write_diagnostics(diagnostics, diagnostics_size, "unsupported scene schema for line_drawing import");
        goto cleanup;
    }

    layout_snapshot = find_layout_snapshot(root);
    if (!cJSON_IsObject(layout_snapshot)) {
        ok = import_legacy_scene_authoring(layout, root, diagnostics, diagnostics_size);
        goto cleanup;
    }

    snapshot_text = cJSON_PrintBuffered(layout_snapshot, 1024, cJSON_True);
    if (!snapshot_text) {
        write_diagnostics(diagnostics, diagnostics_size, "failed to materialize embedded layout snapshot");
        goto cleanup;
    }

    ok = Layout_LoadFromString(layout, snapshot_text);
    if (!ok) {
        write_diagnostics(diagnostics, diagnostics_size, "embedded layout snapshot could not be loaded");
    }

cleanup:
    free(snapshot_text);
    cJSON_Delete(root);
    return ok;
}
