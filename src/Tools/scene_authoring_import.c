#include "Tools/scene_authoring_import.h"

#include "Layout/scene/layout_scene_camera_authoring.h"
#include "Layout/scene/layout_scene_light_authoring.h"

#include <stdio.h>
#include <string.h>

static void copy_text(char* dst, size_t size, const cJSON* node, const char* fallback) {
    const char* text = cJSON_IsString(node) && node->valuestring ? node->valuestring : fallback;
    if (!dst || size == 0u) return;
    snprintf(dst, size, "%s", text ? text : "");
}

static bool read_vec3(const cJSON* node, Vec3* out) {
    const cJSON* x;
    const cJSON* y;
    const cJSON* z;
    if (!cJSON_IsObject(node) || !out) return false;
    x = cJSON_GetObjectItemCaseSensitive(node, "x");
    y = cJSON_GetObjectItemCaseSensitive(node, "y");
    z = cJSON_GetObjectItemCaseSensitive(node, "z");
    if (!cJSON_IsNumber(x) || !cJSON_IsNumber(y) || !cJSON_IsNumber(z)) return false;
    *out = (Vec3){(float)x->valuedouble, (float)y->valuedouble, (float)z->valuedouble};
    return true;
}

static LineDrawingSceneLightKind read_light_kind(const cJSON* node) {
    if (!cJSON_IsString(node) || !node->valuestring) return LINE_DRAWING_SCENE_LIGHT_POINT;
    if (strcmp(node->valuestring, "directional") == 0) return LINE_DRAWING_SCENE_LIGHT_DIRECTIONAL;
    if (strcmp(node->valuestring, "spot") == 0) return LINE_DRAWING_SCENE_LIGHT_SPOT;
    if (strcmp(node->valuestring, "area") == 0) return LINE_DRAWING_SCENE_LIGHT_AREA;
    return LINE_DRAWING_SCENE_LIGHT_POINT;
}

static void parse_paths(const cJSON* root, LineDrawingSceneAuthoringState* state) {
    const cJSON* paths = cJSON_GetObjectItemCaseSensitive(root, "paths");
    if (!cJSON_IsArray(paths)) return;
    for (int i = 0; i < cJSON_GetArraySize(paths) &&
                    state->path_count < LINE_DRAWING_SCENE_AUTHORING_MAX_PATHS; ++i) {
        const cJSON* node = cJSON_GetArrayItem(paths, i);
        const cJSON* points;
        const cJSON* modes;
        LineDrawingScenePath* path;
        if (!cJSON_IsObject(node)) continue;
        path = &state->paths[state->path_count];
        memset(path, 0, sizeof(*path));
        path->duration_seconds = 5.0f;
        copy_text(path->path_id, sizeof(path->path_id),
                  cJSON_GetObjectItemCaseSensitive(node, "path_id"), "");
        if (!path->path_id[0]) continue;
        copy_text(path->label, sizeof(path->label),
                  cJSON_GetObjectItemCaseSensitive(node, "label"), path->path_id);
        copy_text(path->curve_type, sizeof(path->curve_type),
                  cJSON_GetObjectItemCaseSensitive(node, "curve_type"), "bezier");
        copy_text(path->bound_camera_id, sizeof(path->bound_camera_id),
                  cJSON_GetObjectItemCaseSensitive(node, "camera_id"), "");
        copy_text(path->bound_light_id, sizeof(path->bound_light_id),
                  cJSON_GetObjectItemCaseSensitive(node, "light_id"), "");
        {
            const cJSON* role = cJSON_GetObjectItemCaseSensitive(node, "path_kind");
            path->role = LINE_DRAWING_SCENE_PATH_ROLE_GENERIC;
            if (cJSON_IsString(role) && role->valuestring)
                (void)Layout_ScenePathRole_FromName(role->valuestring, &path->role);
        }
        points = cJSON_GetObjectItemCaseSensitive(node, "control_points");
        if (cJSON_IsArray(points)) {
            for (int p = 0; p < cJSON_GetArraySize(points) &&
                            path->control_point_count < LINE_DRAWING_SCENE_AUTHORING_MAX_PATH_POINTS;
                 ++p) {
                Vec3 point;
                if (read_vec3(cJSON_GetArrayItem(points, p), &point))
                    path->control_points[path->control_point_count++] = point;
            }
        }
        if (path->control_point_count == 0u) continue;
        for (size_t m = 0u; m < LINE_DRAWING_SCENE_AUTHORING_MAX_PATH_ANCHORS; ++m)
            path->tangent_modes[m] = LINE_DRAWING_SCENE_PATH_TANGENT_SMOOTH;
        modes = cJSON_GetObjectItemCaseSensitive(node, "tangent_modes");
        if (cJSON_IsArray(modes)) {
            for (int m = 0; m < cJSON_GetArraySize(modes) &&
                            m < LINE_DRAWING_SCENE_AUTHORING_MAX_PATH_ANCHORS; ++m) {
                const cJSON* mode = cJSON_GetArrayItem(modes, m);
                if (cJSON_IsString(mode) && mode->valuestring)
                    (void)Layout_ScenePathTangentMode_FromName(mode->valuestring,
                                                               &path->tangent_modes[m]);
            }
        }
        {
            const cJSON* closed = cJSON_GetObjectItemCaseSensitive(node, "closed");
            const cJSON* playback = cJSON_GetObjectItemCaseSensitive(node, "playback_mode");
            const cJSON* duration = cJSON_GetObjectItemCaseSensitive(node, "duration_seconds");
            const cJSON* normalized = cJSON_GetObjectItemCaseSensitive(node, "normalized_distance");
            const cJSON* playing = cJSON_GetObjectItemCaseSensitive(node, "playing");
            if (cJSON_IsBool(closed)) path->closed = cJSON_IsTrue(closed);
            if (cJSON_IsString(playback) && playback->valuestring)
                (void)Layout_ScenePathPlaybackMode_FromName(playback->valuestring,
                                                            &path->playback_mode);
            if (cJSON_IsNumber(duration) && duration->valuedouble > 0.0)
                path->duration_seconds = (float)duration->valuedouble;
            if (cJSON_IsNumber(normalized)) {
                float value = (float)normalized->valuedouble;
                path->normalized_distance = value < 0.0f ? 0.0f : value > 1.0f ? 1.0f : value;
            }
            if (cJSON_IsBool(playing)) path->playing = cJSON_IsTrue(playing);
        }
        state->path_count++;
    }
}

static void parse_cameras(const cJSON* root, LineDrawingSceneAuthoringState* state) {
    const cJSON* cameras = cJSON_GetObjectItemCaseSensitive(root, "cameras");
    if (!cJSON_IsArray(cameras)) return;
    for (int i = 0; i < cJSON_GetArraySize(cameras) &&
                    state->camera_count < LINE_DRAWING_SCENE_AUTHORING_MAX_CAMERAS; ++i) {
        const cJSON* node = cJSON_GetArrayItem(cameras, i);
        const cJSON* transform;
        const cJSON* orientation;
        const cJSON* value;
        LineDrawingSceneCamera* camera;
        char id[LINE_DRAWING_SCENE_AUTHORING_ID_SIZE];
        char label[LINE_DRAWING_SCENE_AUTHORING_LABEL_SIZE];
        char path_id[LINE_DRAWING_SCENE_AUTHORING_ID_SIZE];
        if (!cJSON_IsObject(node)) continue;
        copy_text(id, sizeof(id), cJSON_GetObjectItemCaseSensitive(node, "camera_id"), "");
        if (!id[0]) continue;
        copy_text(label, sizeof(label), cJSON_GetObjectItemCaseSensitive(node, "label"), id);
        copy_text(path_id, sizeof(path_id), cJSON_GetObjectItemCaseSensitive(node, "path_id"), "");
        camera = &state->cameras[state->camera_count];
        Layout_SceneCamera_SetDefaults(camera, id, label, path_id);
        transform = cJSON_GetObjectItemCaseSensitive(node, "transform");
        (void)read_vec3(cJSON_GetObjectItemCaseSensitive(transform, "position"), &camera->position);
        orientation = cJSON_GetObjectItemCaseSensitive(node, "orientation");
        value = cJSON_GetObjectItemCaseSensitive(orientation, "mode");
        if (cJSON_IsString(value) && value->valuestring)
            (void)Layout_SceneCameraOrientationMode_FromName(value->valuestring,
                                                             &camera->orientation_mode);
        (void)read_vec3(cJSON_GetObjectItemCaseSensitive(orientation, "look_at_target"),
                        &camera->look_at_target);
        (void)read_vec3(cJSON_GetObjectItemCaseSensitive(orientation, "fixed_forward"),
                        &camera->fixed_forward);
        value = cJSON_GetObjectItemCaseSensitive(orientation, "roll_degrees");
        if (cJSON_IsNumber(value)) camera->roll_degrees = (float)value->valuedouble;
        value = cJSON_GetObjectItemCaseSensitive(node, "vertical_fov_degrees");
        if (cJSON_IsNumber(value) && value->valuedouble > 0.0)
            camera->vertical_fov_degrees = (float)value->valuedouble;
        value = cJSON_GetObjectItemCaseSensitive(node, "near_clip");
        if (cJSON_IsNumber(value) && value->valuedouble > 0.0)
            camera->near_clip = (float)value->valuedouble;
        value = cJSON_GetObjectItemCaseSensitive(node, "far_clip");
        if (cJSON_IsNumber(value) && value->valuedouble > camera->near_clip)
            camera->far_clip = (float)value->valuedouble;
        state->camera_count++;
    }
}

static void parse_lights(const cJSON* root, LineDrawingSceneAuthoringState* state) {
    const cJSON* lights = cJSON_GetObjectItemCaseSensitive(root, "lights");
    if (!cJSON_IsArray(lights)) return;
    for (int i = 0; i < cJSON_GetArraySize(lights) &&
                    state->light_count < LINE_DRAWING_SCENE_AUTHORING_MAX_LIGHTS; ++i) {
        const cJSON* node = cJSON_GetArrayItem(lights, i);
        const cJSON* transform;
        const cJSON* value;
        LineDrawingSceneLight* light;
        char id[LINE_DRAWING_SCENE_AUTHORING_ID_SIZE];
        char label[LINE_DRAWING_SCENE_AUTHORING_LABEL_SIZE];
        if (!cJSON_IsObject(node)) continue;
        copy_text(id, sizeof(id), cJSON_GetObjectItemCaseSensitive(node, "light_id"), "");
        if (!id[0]) continue;
        copy_text(label, sizeof(label), cJSON_GetObjectItemCaseSensitive(node, "label"), id);
        light = &state->lights[state->light_count];
        Layout_SceneLight_SetDefaults(light, id, label);
        light->kind = read_light_kind(cJSON_GetObjectItemCaseSensitive(node, "light_type"));
        copy_text(light->path_id, sizeof(light->path_id),
                  cJSON_GetObjectItemCaseSensitive(node, "path_id"), "");
        transform = cJSON_GetObjectItemCaseSensitive(node, "transform");
        (void)read_vec3(cJSON_GetObjectItemCaseSensitive(transform, "position"), &light->position);
        (void)read_vec3(cJSON_GetObjectItemCaseSensitive(node, "direction"), &light->direction);
        if (!read_vec3(cJSON_GetObjectItemCaseSensitive(node, "aim_target"), &light->aim_target))
            light->aim_target = Vec3_Add(light->position, Vec3_Scale(light->direction, 4.0f));
        {
            Vec3 color;
            if (read_vec3(cJSON_GetObjectItemCaseSensitive(node, "color"), &color)) {
                light->color_rgb[0] = color.x;
                light->color_rgb[1] = color.y;
                light->color_rgb[2] = color.z;
            }
        }
        value = cJSON_GetObjectItemCaseSensitive(node, "enabled");
        if (cJSON_IsBool(value)) light->enabled = cJSON_IsTrue(value);
        value = cJSON_GetObjectItemCaseSensitive(node, "position_mode");
        if (cJSON_IsString(value) && value->valuestring)
            (void)Layout_SceneLightPositionMode_FromName(value->valuestring, &light->position_mode);
        value = cJSON_GetObjectItemCaseSensitive(node, "intensity");
        if (cJSON_IsNumber(value) && value->valuedouble > 0.0) light->intensity = (float)value->valuedouble;
        value = cJSON_GetObjectItemCaseSensitive(node, "radius");
        if (cJSON_IsNumber(value) && value->valuedouble >= 0.0) light->radius = (float)value->valuedouble;
        value = cJSON_GetObjectItemCaseSensitive(node, "falloff");
        if (cJSON_IsString(value) && value->valuestring)
            (void)Layout_SceneLightFalloff_FromName(value->valuestring, &light->falloff);
        {
            const cJSON* area = cJSON_GetObjectItemCaseSensitive(node, "area_size");
            const cJSON* cone = cJSON_GetObjectItemCaseSensitive(node, "cone");
            const cJSON* width = cJSON_GetObjectItemCaseSensitive(area, "width");
            const cJSON* height = cJSON_GetObjectItemCaseSensitive(area, "height");
            const cJSON* inner = cJSON_GetObjectItemCaseSensitive(cone, "inner_degrees");
            const cJSON* outer = cJSON_GetObjectItemCaseSensitive(cone, "outer_degrees");
            if (cJSON_IsNumber(width) && width->valuedouble > 0.0) light->area_size.x = (float)width->valuedouble;
            if (cJSON_IsNumber(height) && height->valuedouble > 0.0) light->area_size.y = (float)height->valuedouble;
            if (cJSON_IsNumber(inner) && inner->valuedouble >= 0.0) light->inner_cone_degrees = (float)inner->valuedouble;
            if (cJSON_IsNumber(outer) && outer->valuedouble > 0.0) light->outer_cone_degrees = (float)outer->valuedouble;
        }
        state->light_count++;
    }
}

static void normalize_bindings(LineDrawingSceneAuthoringState* state) {
    for (size_t p = 0u; p < state->path_count; ++p) {
        LineDrawingScenePath* path = &state->paths[p];
        for (size_t c = 0u; c < state->camera_count; ++c) {
            LineDrawingSceneCamera* camera = &state->cameras[c];
            if ((camera->path_id[0] && strcmp(camera->path_id, path->path_id) == 0) ||
                (path->bound_camera_id[0] && strcmp(path->bound_camera_id, camera->camera_id) == 0)) {
                snprintf(camera->path_id, sizeof(camera->path_id), "%s", path->path_id);
                snprintf(path->bound_camera_id, sizeof(path->bound_camera_id), "%s", camera->camera_id);
                path->role = LINE_DRAWING_SCENE_PATH_ROLE_CAMERA;
            }
        }
        for (size_t l = 0u; l < state->light_count; ++l) {
            LineDrawingSceneLight* light = &state->lights[l];
            if ((light->path_id[0] && strcmp(light->path_id, path->path_id) == 0) ||
                (path->bound_light_id[0] && strcmp(path->bound_light_id, light->light_id) == 0)) {
                snprintf(light->path_id, sizeof(light->path_id), "%s", path->path_id);
                snprintf(path->bound_light_id, sizeof(path->bound_light_id), "%s", light->light_id);
                path->role = LINE_DRAWING_SCENE_PATH_ROLE_LIGHT;
            }
        }
    }
}

static void parse_materials(const cJSON* root, LineDrawingSceneAuthoringState* state) {
    const cJSON* materials = cJSON_GetObjectItemCaseSensitive(root, "materials");
    if (!cJSON_IsArray(materials)) return;
    for (int i = 0; i < cJSON_GetArraySize(materials) &&
                    state->material_count < LINE_DRAWING_SCENE_AUTHORING_MAX_MATERIALS; ++i) {
        const cJSON* node = cJSON_GetArrayItem(materials, i);
        const cJSON* rgba;
        LineDrawingSceneMaterial* material;
        if (!cJSON_IsObject(node)) continue;
        material = &state->materials[state->material_count];
        copy_text(material->material_id, sizeof(material->material_id),
                  cJSON_GetObjectItemCaseSensitive(node, "material_id"), "");
        if (!material->material_id[0]) continue;
        copy_text(material->label, sizeof(material->label),
                  cJSON_GetObjectItemCaseSensitive(node, "label"), material->material_id);
        rgba = cJSON_GetObjectItemCaseSensitive(node, "rgba");
        if (cJSON_IsArray(rgba) && cJSON_GetArraySize(rgba) >= 4) {
            for (int channel = 0; channel < 4; ++channel) {
                const cJSON* value = cJSON_GetArrayItem(rgba, channel);
                material->rgba[channel] = cJSON_IsNumber(value)
                    ? (float)value->valuedouble : (channel == 3 ? 1.0f : 0.7f);
            }
        } else {
            material->rgba[0] = material->rgba[1] = material->rgba[2] = 0.7f;
            material->rgba[3] = 1.0f;
        }
        state->material_count++;
    }
}

bool LineDrawingSceneAuthoringImport_ParseCanonical(
    const cJSON* root,
    LineDrawingSceneAuthoringState* out_authoring,
    bool* out_has_records) {
    LineDrawingSceneAuthoringState parsed;
    if (out_has_records) *out_has_records = false;
    if (!cJSON_IsObject(root) || !out_authoring) return false;
    memset(&parsed, 0, sizeof(parsed));
    parse_paths(root, &parsed);
    parse_cameras(root, &parsed);
    parse_lights(root, &parsed);
    parse_materials(root, &parsed);
    normalize_bindings(&parsed);
    if (parsed.path_count == 0u && parsed.camera_count == 0u && parsed.light_count == 0u &&
        parsed.material_count == 0u)
        return true;
    *out_authoring = parsed;
    if (out_has_records) *out_has_records = true;
    return true;
}
