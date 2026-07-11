#include "Tools/canonical_scene_export_authoring.h"

#include <string.h>

static const char* scene_authoring_string_or(const char* value, const char* fallback) {
    return value && value[0] ? value : fallback;
}

static const char* scene_authoring_light_type(LineDrawingSceneLightKind kind) {
    switch (kind) {
        case LINE_DRAWING_SCENE_LIGHT_POINT: return "point";
        case LINE_DRAWING_SCENE_LIGHT_SPOT: return "spot";
        case LINE_DRAWING_SCENE_LIGHT_DIRECTIONAL:
        default: return "directional";
    }
}

static bool scene_authoring_path_exists(const LineDrawingSceneAuthoringState* authoring,
                                        const char* path_id) {
    if (!authoring || !path_id || !path_id[0]) return false;
    for (size_t i = 0u; i < authoring->camera_path_count; ++i) {
        if (strncmp(authoring->camera_paths[i].path_id,
                    path_id,
                    sizeof(authoring->camera_paths[i].path_id)) == 0) {
            return true;
        }
    }
    return false;
}

static bool scene_authoring_camera_exists(cJSON* cameras, const char* camera_id) {
    cJSON* camera = NULL;
    if (!cJSON_IsArray(cameras) || !camera_id || !camera_id[0]) return false;
    cJSON_ArrayForEach(camera, cameras) {
        cJSON* id = cJSON_GetObjectItemCaseSensitive(camera, "camera_id");
        if (cJSON_IsString(id) && id->valuestring &&
            strcmp(id->valuestring, camera_id) == 0) {
            return true;
        }
    }
    return false;
}

static bool scene_authoring_add_vec3(cJSON* parent, const char* key, Vec3 value) {
    cJSON* vec = NULL;
    if (!parent || !key) return false;
    vec = cJSON_CreateObject();
    if (!vec) return false;
    cJSON_AddNumberToObject(vec, "x", value.x);
    cJSON_AddNumberToObject(vec, "y", value.y);
    cJSON_AddNumberToObject(vec, "z", value.z);
    cJSON_AddItemToObject(parent, key, vec);
    return true;
}

static bool scene_authoring_append_material(cJSON* materials,
                                            const LineDrawingSceneMaterial* material,
                                            const char* material_type) {
    cJSON* node = NULL;
    cJSON* rgba = NULL;
    cJSON* extensions = NULL;
    cJSON* line_drawing = NULL;
    if (!materials || !material || !material->material_id[0]) return false;
    node = cJSON_CreateObject();
    rgba = cJSON_CreateArray();
    extensions = cJSON_CreateObject();
    line_drawing = cJSON_CreateObject();
    if (!node || !rgba || !extensions || !line_drawing) {
        cJSON_Delete(node);
        cJSON_Delete(rgba);
        cJSON_Delete(extensions);
        cJSON_Delete(line_drawing);
        return false;
    }
    cJSON_AddItemToArray(materials, node);
    cJSON_AddStringToObject(node, "material_id", material->material_id);
    cJSON_AddStringToObject(node, "material_type", scene_authoring_string_or(material_type, "flat_color"));
    cJSON_AddStringToObject(node, "label", scene_authoring_string_or(material->label, material->material_id));
    cJSON_AddItemToObject(node, "rgba", rgba);
    for (int i = 0; i < 4; ++i) {
        cJSON_AddItemToArray(rgba, cJSON_CreateNumber(material->rgba[i]));
    }
    cJSON_AddItemToObject(node, "extensions", extensions);
    cJSON_AddItemToObject(extensions, "line_drawing", line_drawing);
    cJSON_AddStringToObject(line_drawing, "source", "scene_authoring_state");
    return true;
}

static bool scene_authoring_append_light(cJSON* lights,
                                         const LineDrawingSceneAuthoringState* authoring,
                                         const LineDrawingSceneLight* light) {
    cJSON* node = NULL;
    cJSON* transform = NULL;
    if (!lights || !authoring || !light || !light->light_id[0]) return false;
    node = cJSON_CreateObject();
    transform = cJSON_CreateObject();
    if (!node || !transform) {
        cJSON_Delete(node);
        cJSON_Delete(transform);
        return false;
    }
    cJSON_AddItemToArray(lights, node);
    cJSON_AddStringToObject(node, "light_id", light->light_id);
    cJSON_AddStringToObject(node, "light_type", scene_authoring_light_type(light->kind));
    cJSON_AddStringToObject(node, "label", scene_authoring_string_or(light->label, light->light_id));
    cJSON_AddBoolToObject(node, "enabled", light->enabled);
    if (scene_authoring_path_exists(authoring, light->path_id)) {
        cJSON_AddStringToObject(node, "path_id", light->path_id);
    }
    cJSON_AddItemToObject(node, "transform", transform);
    if (!scene_authoring_add_vec3(transform, "position", light->position) ||
        !scene_authoring_add_vec3(node, "direction", light->direction)) {
        return false;
    }
    return true;
}

static bool scene_authoring_append_camera(cJSON* cameras,
                                          const LineDrawingSceneAuthoringState* authoring,
                                          const char* camera_id,
                                          const char* camera_type,
                                          const char* path_id) {
    cJSON* node = NULL;
    cJSON* transform = NULL;
    if (!cameras || !authoring || !camera_id || !camera_id[0]) return false;
    if (scene_authoring_camera_exists(cameras, camera_id)) return true;
    node = cJSON_CreateObject();
    transform = cJSON_CreateObject();
    if (!node || !transform) {
        cJSON_Delete(node);
        cJSON_Delete(transform);
        return false;
    }
    cJSON_AddItemToArray(cameras, node);
    cJSON_AddStringToObject(node, "camera_id", camera_id);
    cJSON_AddStringToObject(node, "camera_type", scene_authoring_string_or(camera_type, "perspective"));
    if (scene_authoring_path_exists(authoring, path_id)) {
        cJSON_AddStringToObject(node, "path_id", path_id);
    }
    cJSON_AddItemToObject(node, "transform", transform);
    if (!scene_authoring_add_vec3(transform, "position", (Vec3){ 0.0f, 0.0f, 0.0f })) {
        return false;
    }
    return true;
}

static bool scene_authoring_append_path(cJSON* paths,
                                        const LineDrawingSceneCameraPath* path) {
    cJSON* node = NULL;
    cJSON* control_points = NULL;
    const char* path_kind = NULL;
    if (!paths || !path || !path->path_id[0] || path->control_point_count == 0u) {
        return false;
    }
    node = cJSON_CreateObject();
    control_points = cJSON_CreateArray();
    if (!node || !control_points) {
        cJSON_Delete(node);
        cJSON_Delete(control_points);
        return false;
    }
    cJSON_AddItemToArray(paths, node);
    path_kind = path->bound_camera_id[0] ? "camera" : "generic";
    cJSON_AddStringToObject(node, "path_id", path->path_id);
    cJSON_AddStringToObject(node, "path_kind", path_kind);
    cJSON_AddStringToObject(node, "curve_type", scene_authoring_string_or(path->path_kind, "bezier"));
    cJSON_AddStringToObject(node, "label", scene_authoring_string_or(path->label, path->path_id));
    if (path->bound_camera_id[0]) {
        cJSON_AddStringToObject(node, "camera_id", path->bound_camera_id);
    }
    if (path->bound_light_id[0]) {
        cJSON_AddStringToObject(node, "light_id", path->bound_light_id);
    }
    cJSON_AddItemToObject(node, "control_points", control_points);
    for (size_t i = 0u; i < path->control_point_count; ++i) {
        cJSON* point = cJSON_CreateObject();
        if (!point) return false;
        cJSON_AddItemToArray(control_points, point);
        cJSON_AddNumberToObject(point, "x", path->control_points[i].x);
        cJSON_AddNumberToObject(point, "y", path->control_points[i].y);
        cJSON_AddNumberToObject(point, "z", path->control_points[i].z);
    }
    return true;
}

bool LineDrawingCanonicalScene_HasLiveSceneAuthoringRecords(const Layout* layout) {
    const LineDrawingSceneAuthoringState* authoring = NULL;
    if (!layout) return false;
    authoring = &layout->sceneAuthoring;
    return authoring->material_count > 0u ||
           authoring->light_count > 0u ||
           authoring->camera_path_count > 0u;
}

bool LineDrawingCanonicalScene_AppendLiveSceneAuthoringRecords(cJSON* materials,
                                                               cJSON* lights,
                                                               cJSON* cameras,
                                                               cJSON* paths,
                                                               const Layout* layout,
                                                               const char* material_type,
                                                               const char* camera_type) {
    const LineDrawingSceneAuthoringState* authoring = NULL;
    if (!materials || !lights || !cameras || !paths || !layout) return false;
    authoring = &layout->sceneAuthoring;

    for (size_t i = 0u; i < authoring->material_count; ++i) {
        if (!scene_authoring_append_material(materials,
                                             &authoring->materials[i],
                                             material_type)) {
            return false;
        }
    }
    for (size_t i = 0u; i < authoring->camera_path_count; ++i) {
        if (!scene_authoring_append_path(paths, &authoring->camera_paths[i])) return false;
        if (authoring->camera_paths[i].bound_camera_id[0] &&
            !scene_authoring_append_camera(cameras,
                                           authoring,
                                           authoring->camera_paths[i].bound_camera_id,
                                           camera_type,
                                           authoring->camera_paths[i].path_id)) {
            return false;
        }
    }
    for (size_t i = 0u; i < authoring->light_count; ++i) {
        if (!scene_authoring_append_light(lights, authoring, &authoring->lights[i])) return false;
    }

    if (authoring->camera_path_count == 0u &&
        !scene_authoring_append_camera(cameras,
                                       authoring,
                                       "camera_main",
                                       camera_type,
                                       NULL)) {
        return false;
    }

    return true;
}
