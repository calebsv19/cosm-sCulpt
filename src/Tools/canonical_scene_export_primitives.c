#include "Tools/canonical_scene_export_primitives.h"

#include "core_scene.h"

static const char* core_plane_to_string(CoreObjectPlane plane) {
    switch (plane) {
        case CORE_OBJECT_PLANE_YZ: return "yz";
        case CORE_OBJECT_PLANE_XZ: return "xz";
        case CORE_OBJECT_PLANE_XY:
        default: return "xy";
    }
}

static const char* core_dimensional_mode_to_string(CoreObjectDimensionalMode mode) {
    return mode == CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D ? "full_3d" : "plane_locked";
}

static const char* projection_policy_for_plane(CoreObjectPlane plane) {
    switch (plane) {
        case CORE_OBJECT_PLANE_YZ: return "plane_yz_lock";
        case CORE_OBJECT_PLANE_XZ: return "plane_xz_lock";
        case CORE_OBJECT_PLANE_XY:
        default: return "plane_xy_lock";
    }
}

static bool add_vec3_item(cJSON* parent, const char* key, Vec3 value) {
    cJSON* node = NULL;
    if (!parent || !key) return false;
    node = cJSON_CreateObject();
    if (!node) return false;
    cJSON_AddItemToObject(parent, key, node);
    cJSON_AddNumberToObject(node, "x", value.x);
    cJSON_AddNumberToObject(node, "y", value.y);
    cJSON_AddNumberToObject(node, "z", value.z);
    return true;
}

static bool add_plane_frame_item(cJSON* parent, const char* key, const PlaneFrame3* frame) {
    cJSON* node = NULL;
    if (!parent || !key || !frame) return false;
    node = cJSON_CreateObject();
    if (!node) return false;
    cJSON_AddItemToObject(parent, key, node);
    if (!add_vec3_item(node, "origin", frame->origin)) return false;
    if (!add_vec3_item(node, "axisU", frame->axisU)) return false;
    if (!add_vec3_item(node, "axisV", frame->axisV)) return false;
    if (!add_vec3_item(node, "normal", frame->normal)) return false;
    return true;
}

static CoreObjectVec3 core_object_vec3_from_vec3(Vec3 value) {
    CoreObjectVec3 out;
    out.x = value.x;
    out.y = value.y;
    out.z = value.z;
    return out;
}

static void core_scene_frame_from_plane_frame(CoreSceneFrame3* out, const PlaneFrame3* frame) {
    if (!out || !frame) return;
    out->origin = core_object_vec3_from_vec3(frame->origin);
    out->axis_u = core_object_vec3_from_vec3(frame->axisU);
    out->axis_v = core_object_vec3_from_vec3(frame->axisV);
    out->normal = core_object_vec3_from_vec3(frame->normal);
}

static bool add_core_scene_vec3_item(cJSON* parent, const char* key, const CoreObjectVec3* value) {
    cJSON* node = NULL;
    if (!parent || !key || !value) return false;
    node = cJSON_CreateObject();
    if (!node) return false;
    cJSON_AddItemToObject(parent, key, node);
    cJSON_AddNumberToObject(node, "x", value->x);
    cJSON_AddNumberToObject(node, "y", value->y);
    cJSON_AddNumberToObject(node, "z", value->z);
    return true;
}

static bool add_core_scene_frame_item(cJSON* parent, const char* key, const CoreSceneFrame3* frame) {
    cJSON* node = NULL;
    if (!parent || !key || !frame) return false;
    node = cJSON_CreateObject();
    if (!node) return false;
    cJSON_AddItemToObject(parent, key, node);
    if (!add_core_scene_vec3_item(node, "origin", &frame->origin)) return false;
    if (!add_core_scene_vec3_item(node, "axis_u", &frame->axis_u)) return false;
    if (!add_core_scene_vec3_item(node, "axis_v", &frame->axis_v)) return false;
    if (!add_core_scene_vec3_item(node, "normal", &frame->normal)) return false;
    return true;
}

bool LineDrawingCanonicalScene_AddCanonicalPrimitivePayload(cJSON* object_json, const Object3D* object) {
    cJSON* primitive = NULL;
    CoreSceneObjectContract contract;
    if (!object_json || !object) return false;

    core_scene_object_contract_init(&contract);
    contract.object = object->coreMeta;
    if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
        contract.kind = CORE_SCENE_OBJECT_KIND_RECT_PRISM_PRIMITIVE;
        contract.has_rect_prism_primitive = true;
        contract.rect_prism_primitive.width = object->rectPrism.width;
        contract.rect_prism_primitive.height = object->rectPrism.height;
        contract.rect_prism_primitive.depth = object->rectPrism.depth;
        contract.rect_prism_primitive.lock_to_construction_plane = object->rectPrism.lockToConstructionPlane;
        contract.rect_prism_primitive.lock_to_bounds = object->rectPrism.lockToBounds;
        core_scene_frame_from_plane_frame(&contract.rect_prism_primitive.frame, &object->rectPrism.frame);
    } else {
        contract.kind = CORE_SCENE_OBJECT_KIND_PLANE_PRIMITIVE;
        contract.has_plane_primitive = true;
        contract.plane_primitive.width = object->plane.width;
        contract.plane_primitive.height = object->plane.height;
        contract.plane_primitive.lock_to_construction_plane = object->plane.lockToConstructionPlane;
        contract.plane_primitive.lock_to_bounds = object->plane.lockToBounds;
        core_scene_frame_from_plane_frame(&contract.plane_primitive.frame, &object->plane.frame);
    }

    if (core_scene_object_contract_validate(&contract).code != CORE_OK) return false;

    primitive = cJSON_CreateObject();
    if (!primitive) return false;
    cJSON_AddItemToObject(object_json, "primitive", primitive);
    cJSON_AddStringToObject(primitive, "kind", core_scene_object_kind_name(contract.kind));

    if (contract.kind == CORE_SCENE_OBJECT_KIND_RECT_PRISM_PRIMITIVE) {
        cJSON_AddNumberToObject(primitive, "width", contract.rect_prism_primitive.width);
        cJSON_AddNumberToObject(primitive, "height", contract.rect_prism_primitive.height);
        cJSON_AddNumberToObject(primitive, "depth", contract.rect_prism_primitive.depth);
        cJSON_AddBoolToObject(primitive,
                              "lock_to_construction_plane",
                              contract.rect_prism_primitive.lock_to_construction_plane);
        cJSON_AddBoolToObject(primitive, "lock_to_bounds", contract.rect_prism_primitive.lock_to_bounds);
        if (!add_core_scene_frame_item(primitive, "frame", &contract.rect_prism_primitive.frame)) return false;
    } else {
        cJSON_AddNumberToObject(primitive, "width", contract.plane_primitive.width);
        cJSON_AddNumberToObject(primitive, "height", contract.plane_primitive.height);
        cJSON_AddBoolToObject(primitive,
                              "lock_to_construction_plane",
                              contract.plane_primitive.lock_to_construction_plane);
        cJSON_AddBoolToObject(primitive, "lock_to_bounds", contract.plane_primitive.lock_to_bounds);
        if (!add_core_scene_frame_item(primitive, "frame", &contract.plane_primitive.frame)) return false;
    }

    return true;
}

bool LineDrawingCanonicalScene_PopulateScene3DExtension(cJSON* line_drawing_ext, const Layout* layout) {
    cJSON* scene3d = NULL;
    cJSON* bounds = NULL;
    cJSON* construction_plane = NULL;
    cJSON* custom_frame = NULL;

    if (!line_drawing_ext || !layout) return false;

    scene3d = cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(line_drawing_ext, "scene3d"), 1);
    if (!scene3d) scene3d = cJSON_CreateObject();
    if (!scene3d) return false;
    if (!cJSON_ReplaceItemInObjectCaseSensitive(line_drawing_ext, "scene3d", scene3d)) {
        cJSON_AddItemToObject(line_drawing_ext, "scene3d", scene3d);
    }

    bounds = cJSON_CreateObject();
    if (!bounds) return false;
    if (!cJSON_ReplaceItemInObjectCaseSensitive(scene3d, "bounds", bounds)) {
        cJSON_AddItemToObject(scene3d, "bounds", bounds);
    }
    cJSON_AddBoolToObject(bounds, "enabled", layout->scene3d.bounds.enabled);
    cJSON_AddBoolToObject(bounds, "clamp_on_edit", layout->scene3d.bounds.clampOnEdit);
    if (!add_vec3_item(bounds, "min", layout->scene3d.bounds.min)) return false;
    if (!add_vec3_item(bounds, "max", layout->scene3d.bounds.max)) return false;

    construction_plane = cJSON_CreateObject();
    if (!construction_plane) return false;
    if (!cJSON_ReplaceItemInObjectCaseSensitive(scene3d, "construction_plane", construction_plane)) {
        cJSON_AddItemToObject(scene3d, "construction_plane", construction_plane);
    }

    cJSON_AddStringToObject(construction_plane,
                            "mode",
                            layout->scene3d.constructionPlane.mode == CONSTRUCTION_PLANE_MODE_CUSTOM_FRAME
                                ? "custom_frame"
                                : "axis_aligned");
    cJSON_AddStringToObject(construction_plane,
                            "axis",
                            layout->scene3d.constructionPlane.axisAligned.axis == VIEW_PLANE_YZ
                                ? "yz"
                                : layout->scene3d.constructionPlane.axisAligned.axis == VIEW_PLANE_XZ
                                      ? "xz"
                                      : "xy");
    cJSON_AddNumberToObject(construction_plane,
                            "offset",
                            layout->scene3d.constructionPlane.axisAligned.offset);

    custom_frame = cJSON_CreateObject();
    if (!custom_frame) return false;
    if (!cJSON_ReplaceItemInObjectCaseSensitive(construction_plane, "custom_frame", custom_frame)) {
        cJSON_AddItemToObject(construction_plane, "custom_frame", custom_frame);
    }
    if (!add_vec3_item(custom_frame, "origin", layout->scene3d.constructionPlane.customFrame.origin)) return false;
    if (!add_vec3_item(custom_frame, "axisU", layout->scene3d.constructionPlane.customFrame.axisU)) return false;
    if (!add_vec3_item(custom_frame, "axisV", layout->scene3d.constructionPlane.customFrame.axisV)) return false;
    if (!add_vec3_item(custom_frame, "normal", layout->scene3d.constructionPlane.customFrame.normal)) return false;

    return true;
}

cJSON* LineDrawingCanonicalScene_AppendSceneObjectFromCore(cJSON* objects,
                                                           const CoreObject* object,
                                                           const char* object_id,
                                                           const char* geometry_id,
                                                           const char* material_id,
                                                           const char* tag2,
                                                           const char* tag3) {
    cJSON* object_json = NULL;

    if (!objects || !object || !object_id || !geometry_id) return NULL;
    if (core_object_validate(object).code != CORE_OK) return NULL;

    object_json = cJSON_CreateObject();
    if (!object_json) return NULL;
    cJSON_AddItemToArray(objects, object_json);

    cJSON_AddStringToObject(object_json, "object_id", object_id);
    cJSON_AddStringToObject(object_json, "object_type", object->object_type);
    cJSON_AddStringToObject(object_json,
                            "space_mode_intent",
                            object->dimensional_mode == CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D ? "3d" : "2d");
    cJSON_AddStringToObject(object_json,
                            "dimensional_mode",
                            core_dimensional_mode_to_string(object->dimensional_mode));
    if (object->dimensional_mode == CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED) {
        cJSON_AddStringToObject(object_json, "locked_plane", core_plane_to_string(object->locked_plane));
        cJSON_AddStringToObject(object_json,
                                "projection_policy",
                                projection_policy_for_plane(object->locked_plane));
        cJSON_AddStringToObject(object_json, "extrusion_policy", "none");
    } else {
        cJSON_AddStringToObject(object_json, "projection_policy", "none");
        cJSON_AddStringToObject(object_json, "extrusion_policy", "none");
    }

    {
        cJSON* transform = cJSON_CreateObject();
        cJSON* position = cJSON_CreateObject();
        cJSON* rotation = cJSON_CreateObject();
        cJSON* scale = cJSON_CreateObject();
        cJSON* geometry_ref = cJSON_CreateObject();
        cJSON* tags = cJSON_CreateArray();
        cJSON* flags = cJSON_CreateObject();
        cJSON* material_ref = NULL;
        if (!transform || !position || !rotation || !scale || !geometry_ref || !tags || !flags) return NULL;
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
            if (!material_ref) return NULL;
            cJSON_AddItemToObject(object_json, "material_ref", material_ref);
            cJSON_AddStringToObject(material_ref, "id", material_id);
        }
        cJSON_AddItemToArray(tags, cJSON_CreateString("authoring"));
        cJSON_AddItemToArray(tags, cJSON_CreateString("line_drawing"));
        if (tag2) cJSON_AddItemToArray(tags, cJSON_CreateString(tag2));
        if (tag3) cJSON_AddItemToArray(tags, cJSON_CreateString(tag3));
        cJSON_AddBoolToObject(flags, "visible", object->flags.visible);
        cJSON_AddBoolToObject(flags, "locked", object->flags.locked);
        cJSON_AddBoolToObject(flags, "selectable", object->flags.selectable);
    }
    return object_json;
}

bool LineDrawingCanonicalScene_AddPrimitiveExtensionPayload(cJSON* object_extensions,
                                                            const Object3D* object) {
    cJSON* line_drawing_ext = NULL;
    cJSON* primitive = NULL;
    if (!object_extensions || !object) return false;

    line_drawing_ext = cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(object_extensions, "line_drawing"), 1);
    if (!line_drawing_ext) line_drawing_ext = cJSON_CreateObject();
    if (!line_drawing_ext) return false;
    if (!cJSON_ReplaceItemInObjectCaseSensitive(object_extensions, "line_drawing", line_drawing_ext)) {
        cJSON_AddItemToObject(object_extensions, "line_drawing", line_drawing_ext);
    }

    cJSON_AddStringToObject(line_drawing_ext, "geometry_source", "objects3d");
    cJSON_AddStringToObject(line_drawing_ext, "source_lane", "objects3d");
    cJSON_AddNumberToObject(line_drawing_ext, "layout_object_id", (double)object->objectId);
    cJSON_AddStringToObject(line_drawing_ext,
                            "primitive_kind",
                            object->kind == OBJECT3D_KIND_RECT_PRISM ? "rect_prism" : "plane");

    primitive = cJSON_CreateObject();
    if (!primitive) return false;
    if (!cJSON_ReplaceItemInObjectCaseSensitive(line_drawing_ext, "primitive_payload", primitive)) {
        cJSON_AddItemToObject(line_drawing_ext, "primitive_payload", primitive);
    }
    if (object->kind == OBJECT3D_KIND_RECT_PRISM) {
        cJSON_AddNumberToObject(primitive, "width", object->rectPrism.width);
        cJSON_AddNumberToObject(primitive, "height", object->rectPrism.height);
        cJSON_AddNumberToObject(primitive, "depth", object->rectPrism.depth);
        cJSON_AddBoolToObject(primitive, "lock_to_construction_plane", object->rectPrism.lockToConstructionPlane);
        cJSON_AddBoolToObject(primitive, "lock_to_bounds", object->rectPrism.lockToBounds);
        if (!add_plane_frame_item(primitive, "frame", &object->rectPrism.frame)) return false;
    } else {
        cJSON_AddNumberToObject(primitive, "width", object->plane.width);
        cJSON_AddNumberToObject(primitive, "height", object->plane.height);
        cJSON_AddBoolToObject(primitive, "lock_to_construction_plane", object->plane.lockToConstructionPlane);
        cJSON_AddBoolToObject(primitive, "lock_to_bounds", object->plane.lockToBounds);
        if (!add_plane_frame_item(primitive, "frame", &object->plane.frame)) return false;
    }
    return true;
}
