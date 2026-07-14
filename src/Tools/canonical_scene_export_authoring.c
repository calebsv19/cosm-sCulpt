#include "Tools/canonical_scene_export_authoring.h"
#include "Layout/scene/layout_scene_camera_authoring.h"
#include "Layout/scene/layout_scene_light_authoring.h"
#include "Layout/scene/layout_scene_path_edit.h"

#include <string.h>

static const char* scene_authoring_string_or(const char* value, const char* fallback) {
    return value && value[0] ? value : fallback;
}

static const char* scene_authoring_light_type(LineDrawingSceneLightKind kind) {
    switch (kind) {
        case LINE_DRAWING_SCENE_LIGHT_POINT: return "point";
        case LINE_DRAWING_SCENE_LIGHT_SPOT: return "spot";
        case LINE_DRAWING_SCENE_LIGHT_AREA: return "area";
        case LINE_DRAWING_SCENE_LIGHT_DIRECTIONAL:
        default: return "directional";
    }
}

static bool scene_authoring_path_exists(const LineDrawingSceneAuthoringState* authoring,
                                        const char* path_id) {
    if (!authoring || !path_id || !path_id[0]) return false;
    for (size_t i = 0u; i < authoring->path_count; ++i) {
        if (strncmp(authoring->paths[i].path_id,
                    path_id,
                    sizeof(authoring->paths[i].path_id)) == 0) {
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
    cJSON* area_size = NULL;
    cJSON* cone = NULL;
    const LineDrawingScenePath* path = NULL;
    Vec3 position;
    Vec3 direction;
    if (!lights || !authoring || !light || !light->light_id[0]) return false;
    node = cJSON_CreateObject();
    transform = cJSON_CreateObject();
    area_size = cJSON_CreateObject();
    cone = cJSON_CreateObject();
    if (!node || !transform || !area_size || !cone) {
        cJSON_Delete(node);
        cJSON_Delete(transform);
        cJSON_Delete(area_size);
        cJSON_Delete(cone);
        return false;
    }
    path = Layout_SceneAuthoringState_FindPathByIdConst(authoring, light->path_id);
    if (!Layout_SceneLight_EvaluatePositionAtNormalizedDistance(
            light, path, path ? path->normalized_distance : 0.0f, &position)) {
        position = Layout_SceneLight_EffectivePosition(light, path);
    }
    direction = Vec3_Sub(light->aim_target, position);
    if (Vec3_Length(direction) < 0.0001f) {
        direction = Layout_SceneLight_EffectiveDirection(light, path);
    } else {
        direction = Vec3_Normalize(direction);
    }
    cJSON_AddItemToArray(lights, node);
    cJSON_AddStringToObject(node, "light_id", light->light_id);
    cJSON_AddStringToObject(node, "light_type", scene_authoring_light_type(light->kind));
    cJSON_AddStringToObject(node, "label", scene_authoring_string_or(light->label, light->light_id));
    cJSON_AddBoolToObject(node, "enabled", light->enabled);
    cJSON_AddStringToObject(node, "position_mode",
                            Layout_SceneLightPositionMode_Name(light->position_mode));
    if (scene_authoring_path_exists(authoring, light->path_id)) {
        cJSON_AddStringToObject(node, "path_id", light->path_id);
    }
    cJSON_AddItemToObject(node, "transform", transform);
    cJSON_AddItemToObject(node, "area_size", area_size);
    cJSON_AddNumberToObject(area_size, "width", light->area_size.x);
    cJSON_AddNumberToObject(area_size, "height", light->area_size.y);
    cJSON_AddItemToObject(node, "cone", cone);
    cJSON_AddNumberToObject(cone, "inner_degrees", light->inner_cone_degrees);
    cJSON_AddNumberToObject(cone, "outer_degrees", light->outer_cone_degrees);
    cJSON_AddNumberToObject(node, "intensity", light->intensity);
    cJSON_AddNumberToObject(node, "radius", light->radius);
    cJSON_AddStringToObject(node, "falloff", Layout_SceneLightFalloff_Name(light->falloff));
    if (!scene_authoring_add_vec3(transform, "position", position) ||
        !scene_authoring_add_vec3(node, "direction", direction) ||
        !scene_authoring_add_vec3(node, "aim_target", light->aim_target) ||
        !scene_authoring_add_vec3(node, "color", (Vec3){light->color_rgb[0],
                                                         light->color_rgb[1],
                                                         light->color_rgb[2]})) {
        return false;
    }
    return true;
}

static bool scene_authoring_append_camera(cJSON* cameras,
                                          const LineDrawingSceneAuthoringState* authoring,
                                          const LineDrawingSceneCamera* camera,
                                          const LineDrawingScenePath* path,
                                          const char* camera_type,
                                          const char* fallback_camera_id) {
    cJSON* node = NULL;
    cJSON* transform = NULL;
    cJSON* orientation = NULL;
    LineDrawingSceneCamera fallback = {0};
    LineDrawingSceneCameraPose pose = {0};
    const char* camera_id = camera ? camera->camera_id : fallback_camera_id;
    if (!cameras || !authoring || !camera_id || !camera_id[0]) return false;
    if (!camera) {
        Layout_SceneCamera_SetDefaults(&fallback, camera_id, camera_id,
                                       path ? path->path_id : "");
        camera = &fallback;
    }
    if (scene_authoring_camera_exists(cameras, camera_id)) return true;
    node = cJSON_CreateObject();
    transform = cJSON_CreateObject();
    orientation = cJSON_CreateObject();
    if (!node || !transform || !orientation ||
        !Layout_SceneCamera_EvaluatePoseAtNormalizedDistance(
            camera, path, path ? path->normalized_distance : 0.0f, &pose)) {
        cJSON_Delete(node);
        cJSON_Delete(transform);
        cJSON_Delete(orientation);
        return false;
    }
    cJSON_AddItemToArray(cameras, node);
    cJSON_AddStringToObject(node, "camera_id", camera_id);
    cJSON_AddStringToObject(node, "camera_type", scene_authoring_string_or(camera_type, "perspective"));
    cJSON_AddStringToObject(node, "label", scene_authoring_string_or(camera->label, camera_id));
    if (scene_authoring_path_exists(authoring, camera->path_id)) {
        cJSON_AddStringToObject(node, "path_id", camera->path_id);
    }
    cJSON_AddItemToObject(node, "transform", transform);
    cJSON_AddItemToObject(node, "orientation", orientation);
    cJSON_AddStringToObject(orientation, "mode",
                            Layout_SceneCameraOrientationMode_Name(camera->orientation_mode));
    cJSON_AddNumberToObject(orientation, "roll_degrees", camera->roll_degrees);
    cJSON_AddNumberToObject(node, "vertical_fov_degrees", camera->vertical_fov_degrees);
    cJSON_AddNumberToObject(node, "near_clip", camera->near_clip);
    cJSON_AddNumberToObject(node, "far_clip", camera->far_clip);
    if (!scene_authoring_add_vec3(transform, "position", pose.position) ||
        !scene_authoring_add_vec3(transform, "forward", pose.forward) ||
        !scene_authoring_add_vec3(transform, "up", pose.up) ||
        !scene_authoring_add_vec3(orientation, "look_at_target", camera->look_at_target) ||
        !scene_authoring_add_vec3(orientation, "fixed_forward", camera->fixed_forward)) {
        return false;
    }
    return true;
}

static bool scene_authoring_append_path(cJSON* paths,
                                        const LineDrawingScenePath* path) {
    cJSON* node = NULL;
    cJSON* control_points = NULL;
    cJSON* tangent_modes = NULL;
    if (!paths || !path || !path->path_id[0] || path->control_point_count == 0u) {
        return false;
    }
    node = cJSON_CreateObject();
    control_points = cJSON_CreateArray();
    tangent_modes = cJSON_CreateArray();
    if (!node || !control_points || !tangent_modes) {
        cJSON_Delete(node);
        cJSON_Delete(control_points);
        cJSON_Delete(tangent_modes);
        return false;
    }
    cJSON_AddItemToArray(paths, node);
    cJSON_AddStringToObject(node, "path_id", path->path_id);
    cJSON_AddStringToObject(node, "path_kind", Layout_ScenePathRole_Name(path->role));
    cJSON_AddStringToObject(node, "curve_type", scene_authoring_string_or(path->curve_type, "bezier"));
    cJSON_AddStringToObject(node, "label", scene_authoring_string_or(path->label, path->path_id));
    if (path->bound_camera_id[0]) {
        cJSON_AddStringToObject(node, "camera_id", path->bound_camera_id);
    }
    if (path->bound_light_id[0]) {
        cJSON_AddStringToObject(node, "light_id", path->bound_light_id);
    }
    cJSON_AddBoolToObject(node, "closed", path->closed);
    cJSON_AddStringToObject(node, "playback_mode",
                            Layout_ScenePathPlaybackMode_Name(path->playback_mode));
    cJSON_AddNumberToObject(node, "duration_seconds", path->duration_seconds);
    cJSON_AddNumberToObject(node, "normalized_distance", path->normalized_distance);
    cJSON_AddBoolToObject(node, "playing", path->playing);
    cJSON_AddItemToObject(node, "control_points", control_points);
    for (size_t i = 0u; i < path->control_point_count; ++i) {
        cJSON* point = cJSON_CreateObject();
        if (!point) return false;
        cJSON_AddItemToArray(control_points, point);
        cJSON_AddNumberToObject(point, "x", path->control_points[i].x);
        cJSON_AddNumberToObject(point, "y", path->control_points[i].y);
        cJSON_AddNumberToObject(point, "z", path->control_points[i].z);
    }
    cJSON_AddItemToObject(node, "tangent_modes", tangent_modes);
    for (size_t i = 0u; i < Layout_ScenePathEdit_AnchorCount(path); ++i) {
        cJSON_AddItemToArray(tangent_modes,
                             cJSON_CreateString(Layout_ScenePathTangentMode_Name(
                                 Layout_ScenePathEdit_AnchorMode(path, i))));
    }
    return true;
}

bool LineDrawingCanonicalScene_HasLiveSceneAuthoringRecords(const Layout* layout) {
    const LineDrawingSceneAuthoringState* authoring = NULL;
    if (!layout) return false;
    authoring = &layout->sceneAuthoring;
    return authoring->material_count > 0u ||
           authoring->light_count > 0u ||
           authoring->camera_count > 0u ||
           authoring->path_count > 0u;
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
    for (size_t i = 0u; i < authoring->path_count; ++i) {
        if (!scene_authoring_append_path(paths, &authoring->paths[i])) return false;
    }
    for (size_t i = 0u; i < authoring->camera_count; ++i) {
        const LineDrawingSceneCamera* camera = &authoring->cameras[i];
        const LineDrawingScenePath* path =
            Layout_SceneAuthoringState_FindPathByIdConst(authoring, camera->path_id);
        if (!scene_authoring_append_camera(cameras, authoring, camera, path,
                                           camera_type, NULL)) {
            return false;
        }
    }
    for (size_t i = 0u; i < authoring->light_count; ++i) {
        if (!scene_authoring_append_light(lights, authoring, &authoring->lights[i])) return false;
    }

    if (authoring->camera_count == 0u &&
        !scene_authoring_append_camera(cameras,
                                       authoring,
                                       NULL,
                                       NULL,
                                       camera_type,
                                       "camera_main")) {
        return false;
    }

    return true;
}
