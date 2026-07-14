#include "Layout/scene/layout_scene_light_authoring.h"
#include "Layout/scene/layout_scene_path_traversal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct LineDrawingSceneLightColorPreset {
    float rgb[3];
} LineDrawingSceneLightColorPreset;

static const LineDrawingSceneLightColorPreset k_light_colors[] = {
    {{1.00f, 1.00f, 1.00f}},
    {{1.00f, 0.78f, 0.56f}},
    {{0.62f, 0.78f, 1.00f}},
    {{0.72f, 1.00f, 0.70f}}
};

static LineDrawingSceneLight* selected_light(LineDrawingSceneAuthoringState* state) {
    if (!state || state->selected_kind != LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT ||
        state->selected_index >= state->light_count) return NULL;
    return &state->lights[state->selected_index];
}

/* Initializes a renderer-neutral editable light record with stable P5 defaults. */
void Layout_SceneLight_SetDefaults(LineDrawingSceneLight* light,
                                   const char* light_id,
                                   const char* label) {
    if (!light) return;
    memset(light, 0, sizeof(*light));
    snprintf(light->light_id, sizeof(light->light_id), "%s", light_id ? light_id : "light");
    snprintf(light->label, sizeof(light->label), "%s", label ? label : light->light_id);
    light->kind = LINE_DRAWING_SCENE_LIGHT_POINT;
    light->position = (Vec3){0.0f, -4.0f, 4.0f};
    light->direction = (Vec3){0.0f, 0.0f, -1.0f};
    light->aim_target = (Vec3){0.0f, -4.0f, 0.0f};
    light->enabled = true;
    light->position_mode = LINE_DRAWING_SCENE_LIGHT_POSITION_INDEPENDENT;
    light->color_rgb[0] = 1.0f;
    light->color_rgb[1] = 1.0f;
    light->color_rgb[2] = 1.0f;
    light->intensity = 1.0f;
    light->radius = 0.25f;
    light->area_size = (Vec2){2.0f, 2.0f};
    light->inner_cone_degrees = 25.0f;
    light->outer_cone_degrees = 40.0f;
    light->falloff = LINE_DRAWING_SCENE_LIGHT_FALLOFF_INVERSE_SQUARE;
}

const char* Layout_SceneLightPositionMode_Name(LineDrawingSceneLightPositionMode mode) {
    return mode == LINE_DRAWING_SCENE_LIGHT_POSITION_PATH_START ? "path_start" : "independent";
}

bool Layout_SceneLightPositionMode_FromName(const char* name,
                                           LineDrawingSceneLightPositionMode* out_mode) {
    if (!name || !out_mode) return false;
    if (strcmp(name, "independent") == 0) {
        *out_mode = LINE_DRAWING_SCENE_LIGHT_POSITION_INDEPENDENT;
        return true;
    }
    if (strcmp(name, "path_start") == 0) {
        *out_mode = LINE_DRAWING_SCENE_LIGHT_POSITION_PATH_START;
        return true;
    }
    return false;
}

const char* Layout_SceneLightFalloff_Name(LineDrawingSceneLightFalloff falloff) {
    switch (falloff) {
        case LINE_DRAWING_SCENE_LIGHT_FALLOFF_LINEAR: return "linear";
        case LINE_DRAWING_SCENE_LIGHT_FALLOFF_CONSTANT: return "constant";
        case LINE_DRAWING_SCENE_LIGHT_FALLOFF_INVERSE_SQUARE:
        default: return "inverse_square";
    }
}

bool Layout_SceneLightFalloff_FromName(const char* name,
                                      LineDrawingSceneLightFalloff* out_falloff) {
    if (!name || !out_falloff) return false;
    if (strcmp(name, "inverse_square") == 0) {
        *out_falloff = LINE_DRAWING_SCENE_LIGHT_FALLOFF_INVERSE_SQUARE;
        return true;
    }
    if (strcmp(name, "linear") == 0) {
        *out_falloff = LINE_DRAWING_SCENE_LIGHT_FALLOFF_LINEAR;
        return true;
    }
    if (strcmp(name, "constant") == 0) {
        *out_falloff = LINE_DRAWING_SCENE_LIGHT_FALLOFF_CONSTANT;
        return true;
    }
    return false;
}

Vec3 Layout_SceneLight_EffectivePosition(const LineDrawingSceneLight* light,
                                        const LineDrawingScenePath* path) {
    if (!light) return (Vec3){0};
    if (light->position_mode == LINE_DRAWING_SCENE_LIGHT_POSITION_PATH_START && path &&
        path->control_point_count > 0u) {
        return path->control_points[0];
    }
    return light->position;
}

bool Layout_SceneLight_EvaluatePositionAtNormalizedDistance(
    const LineDrawingSceneLight* light,
    const LineDrawingScenePath* path,
    float normalized_distance,
    Vec3* out_position) {
    LineDrawingScenePathTraversalTable table = {0};
    LineDrawingScenePathTraversalSample sample = {0};
    if (!light || !out_position) return false;
    if (light->position_mode != LINE_DRAWING_SCENE_LIGHT_POSITION_PATH_START || !path) {
        *out_position = light->position;
        return true;
    }
    if (!Layout_ScenePathTraversal_Build(path, &table) ||
        !Layout_ScenePathTraversal_EvaluateNormalized(&table,
                                                      normalized_distance,
                                                      path->playback_mode,
                                                      &sample)) return false;
    *out_position = sample.world;
    return true;
}

Vec3 Layout_SceneLight_EffectiveDirection(const LineDrawingSceneLight* light,
                                          const LineDrawingScenePath* path) {
    Vec3 direction;
    if (!light) return (Vec3){0.0f, 0.0f, -1.0f};
    direction = Vec3_Sub(light->aim_target, Layout_SceneLight_EffectivePosition(light, path));
    if (Vec3_Length(direction) < 0.0001f) {
        direction = Vec3_Normalize(light->direction);
    }
    if (Vec3_Length(direction) < 0.0001f) direction = (Vec3){0.0f, 0.0f, -1.0f};
    return Vec3_Normalize(direction);
}

Vec3 Layout_SceneLight_AimPoint(const LineDrawingSceneLight* light,
                               const LineDrawingScenePath* path) {
    (void)path;
    return light ? light->aim_target : (Vec3){0};
}

bool Layout_SceneLight_SetAimPoint(LineDrawingSceneLight* light,
                                  const LineDrawingScenePath* path,
                                  Vec3 aim_point) {
    Vec3 direction;
    if (!light) return false;
    direction = Vec3_Sub(aim_point, Layout_SceneLight_EffectivePosition(light, path));
    if (Vec3_Length(direction) < 0.0001f) return false;
    light->aim_target = aim_point;
    light->direction = Vec3_Normalize(direction);
    return true;
}

bool Layout_SceneAuthoringState_CycleSelectedLightPositionMode(
    LineDrawingSceneAuthoringState* state) {
    LineDrawingSceneLight* light = selected_light(state);
    if (!light || light->path_id[0] == '\0') return false;
    light->position_mode = light->position_mode == LINE_DRAWING_SCENE_LIGHT_POSITION_INDEPENDENT
        ? LINE_DRAWING_SCENE_LIGHT_POSITION_PATH_START
        : LINE_DRAWING_SCENE_LIGHT_POSITION_INDEPENDENT;
    return true;
}

bool Layout_SceneAuthoringState_CycleSelectedLightColor(LineDrawingSceneAuthoringState* state) {
    LineDrawingSceneLight* light = selected_light(state);
    size_t next = 0u;
    if (!light) return false;
    for (size_t i = 0u; i < sizeof(k_light_colors) / sizeof(k_light_colors[0]); ++i) {
        if (fabsf(light->color_rgb[0] - k_light_colors[i].rgb[0]) < 0.001f &&
            fabsf(light->color_rgb[1] - k_light_colors[i].rgb[1]) < 0.001f &&
            fabsf(light->color_rgb[2] - k_light_colors[i].rgb[2]) < 0.001f) {
            next = (i + 1u) % (sizeof(k_light_colors) / sizeof(k_light_colors[0]));
            break;
        }
    }
    memcpy(light->color_rgb, k_light_colors[next].rgb, sizeof(light->color_rgb));
    return true;
}

bool Layout_SceneAuthoringState_CycleSelectedLightIntensity(
    LineDrawingSceneAuthoringState* state) {
    LineDrawingSceneLight* light = selected_light(state);
    if (!light) return false;
    light->intensity = light->intensity < 2.0f ? 2.0f : light->intensity < 5.0f ? 5.0f : 1.0f;
    return true;
}

bool Layout_SceneAuthoringState_CycleSelectedLightRadiusOrSize(
    LineDrawingSceneAuthoringState* state) {
    LineDrawingSceneLight* light = selected_light(state);
    if (!light) return false;
    if (light->kind == LINE_DRAWING_SCENE_LIGHT_AREA) {
        const float next = light->area_size.x < 4.0f ? 4.0f : light->area_size.x < 8.0f ? 8.0f : 2.0f;
        light->area_size = (Vec2){next, next};
    } else {
        light->radius = light->radius < 0.5f ? 0.5f : light->radius < 1.0f ? 1.0f : 0.25f;
    }
    return true;
}

bool Layout_SceneAuthoringState_CycleSelectedLightCone(LineDrawingSceneAuthoringState* state) {
    LineDrawingSceneLight* light = selected_light(state);
    if (!light) return false;
    if (light->outer_cone_degrees < 55.0f) {
        light->inner_cone_degrees = 35.0f;
        light->outer_cone_degrees = 60.0f;
    } else {
        light->inner_cone_degrees = 25.0f;
        light->outer_cone_degrees = 40.0f;
    }
    return true;
}

bool Layout_SceneAuthoringState_CycleSelectedLightFalloff(
    LineDrawingSceneAuthoringState* state) {
    LineDrawingSceneLight* light = selected_light(state);
    if (!light) return false;
    light->falloff = light->falloff == LINE_DRAWING_SCENE_LIGHT_FALLOFF_INVERSE_SQUARE
        ? LINE_DRAWING_SCENE_LIGHT_FALLOFF_LINEAR
        : light->falloff == LINE_DRAWING_SCENE_LIGHT_FALLOFF_LINEAR
            ? LINE_DRAWING_SCENE_LIGHT_FALLOFF_CONSTANT
            : LINE_DRAWING_SCENE_LIGHT_FALLOFF_INVERSE_SQUARE;
    return true;
}
