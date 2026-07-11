#include "Layout/scene/layout_scene_authoring.h"

#include <stdio.h>
#include <string.h>

typedef struct LineDrawingSceneAuthoringPaletteEntry {
    const char* label;
    float rgba[4];
} LineDrawingSceneAuthoringPaletteEntry;

static const LineDrawingSceneAuthoringPaletteEntry k_scene_authoring_material_palette[] = {
    { "Default Matte", { 0.72f, 0.72f, 0.68f, 1.0f } },
    { "Cool Blue",    { 0.45f, 0.62f, 0.80f, 1.0f } },
    { "Warm Clay",    { 0.78f, 0.50f, 0.34f, 1.0f } },
    { "Soft Green",   { 0.48f, 0.70f, 0.52f, 1.0f } },
    { "Graphite",     { 0.34f, 0.36f, 0.40f, 1.0f } }
};

static void ld_scene_authoring_copy_text(char* dst, size_t dst_size, const char* src) {
    size_t len = 0u;
    if (!dst || dst_size == 0u) return;
    if (!src) src = "";
    len = strlen(src);
    if (len >= dst_size) {
        len = dst_size - 1u;
    }
    if (len > 0u && dst != src) {
        memcpy(dst, src, len);
    }
    dst[len] = '\0';
}

static bool ld_scene_authoring_same_rgba(const float a[4], const float b[4]) {
    const float eps = 0.0001f;
    for (int i = 0; i < 4; ++i) {
        const float diff = a[i] - b[i];
        if (diff > eps || diff < -eps) return false;
    }
    return true;
}

void Layout_SceneAuthoringState_ClearSelection(LineDrawingSceneAuthoringState* state) {
    if (!state) return;
    state->selected_kind = LINE_DRAWING_SCENE_AUTHORING_SELECTION_NONE;
    state->selected_index = 0u;
}

void Layout_SceneAuthoringState_Init(LineDrawingSceneAuthoringState* state) {
    LineDrawingSceneLight* light = NULL;
    LineDrawingSceneCameraPath* path = NULL;
    LineDrawingSceneMaterial* material = NULL;
    if (!state) return;
    memset(state, 0, sizeof(*state));

    state->light_count = 1u;
    light = &state->lights[0];
    ld_scene_authoring_copy_text(light->light_id, sizeof(light->light_id), "light_key");
    ld_scene_authoring_copy_text(light->label, sizeof(light->label), "Key Light");
    light->kind = LINE_DRAWING_SCENE_LIGHT_DIRECTIONAL;
    light->position = (Vec3){ 0.0f, -6.0f, 8.0f };
    light->direction = (Vec3){ 0.2f, 0.4f, -1.0f };
    ld_scene_authoring_copy_text(light->path_id, sizeof(light->path_id), "path_light_key");
    light->enabled = true;

    state->camera_path_count = 1u;
    path = &state->camera_paths[0];
    ld_scene_authoring_copy_text(path->path_id, sizeof(path->path_id), "path_camera_main");
    ld_scene_authoring_copy_text(path->label, sizeof(path->label), "Main Camera Path");
    ld_scene_authoring_copy_text(path->path_kind, sizeof(path->path_kind), "bezier");
    path->control_point_count = 3u;
    path->control_points[0] = (Vec3){ -8.0f, -8.0f, 6.0f };
    path->control_points[1] = (Vec3){ 0.0f, -10.0f, 8.0f };
    path->control_points[2] = (Vec3){ 8.0f, -8.0f, 6.0f };
    ld_scene_authoring_copy_text(path->bound_camera_id, sizeof(path->bound_camera_id), "camera_main");

    state->material_count = 1u;
    material = &state->materials[0];
    ld_scene_authoring_copy_text(material->material_id, sizeof(material->material_id), "mat_default");
    ld_scene_authoring_copy_text(material->label, sizeof(material->label), "Default Matte");
    material->rgba[0] = 0.72f;
    material->rgba[1] = 0.72f;
    material->rgba[2] = 0.68f;
    material->rgba[3] = 1.0f;

    Layout_SceneAuthoringState_ClearSelection(state);
}

bool Layout_SceneAuthoringState_Select(LineDrawingSceneAuthoringState* state,
                                       LineDrawingSceneAuthoringSelectionKind kind,
                                       size_t index) {
    if (!state) return false;
    switch (kind) {
        case LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT:
            if (index >= state->light_count) return false;
            break;
        case LINE_DRAWING_SCENE_AUTHORING_SELECTION_CAMERA_PATH:
            if (index >= state->camera_path_count) return false;
            break;
        case LINE_DRAWING_SCENE_AUTHORING_SELECTION_MATERIAL:
            if (index >= state->material_count) return false;
            break;
        case LINE_DRAWING_SCENE_AUTHORING_SELECTION_NONE:
            Layout_SceneAuthoringState_ClearSelection(state);
            return true;
        default:
            return false;
    }
    state->selected_kind = kind;
    state->selected_index = index;
    return true;
}

bool Layout_SceneAuthoringState_AddDefaultLight(LineDrawingSceneAuthoringState* state,
                                                size_t* out_index) {
    LineDrawingSceneLight* light = NULL;
    size_t index = 0u;
    if (!state || state->light_count >= LINE_DRAWING_SCENE_AUTHORING_MAX_LIGHTS) return false;
    index = state->light_count++;
    light = &state->lights[index];
    memset(light, 0, sizeof(*light));
    snprintf(light->light_id, sizeof(light->light_id), "light_%03zu", index + 1u);
    snprintf(light->label, sizeof(light->label), "Light %zu", index + 1u);
    light->kind = LINE_DRAWING_SCENE_LIGHT_POINT;
    light->position = (Vec3){ 0.0f, -4.0f, 4.0f };
    light->direction = (Vec3){ 0.0f, 0.0f, -1.0f };
    light->enabled = true;
    (void)Layout_SceneAuthoringState_Select(state,
                                            LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT,
                                            index);
    if (out_index) *out_index = index;
    return true;
}

bool Layout_SceneAuthoringState_AddDefaultCameraPath(LineDrawingSceneAuthoringState* state,
                                                     size_t* out_index) {
    LineDrawingSceneCameraPath* path = NULL;
    size_t index = 0u;
    if (!state ||
        state->camera_path_count >= LINE_DRAWING_SCENE_AUTHORING_MAX_CAMERA_PATHS) {
        return false;
    }
    index = state->camera_path_count++;
    path = &state->camera_paths[index];
    memset(path, 0, sizeof(*path));
    snprintf(path->path_id, sizeof(path->path_id), "path_camera_%03zu", index + 1u);
    snprintf(path->label, sizeof(path->label), "Camera Path %zu", index + 1u);
    ld_scene_authoring_copy_text(path->path_kind, sizeof(path->path_kind), "bezier");
    path->control_point_count = 3u;
    path->control_points[0] = (Vec3){ -6.0f, -6.0f, 5.0f };
    path->control_points[1] = (Vec3){ 0.0f, -8.0f, 7.0f };
    path->control_points[2] = (Vec3){ 6.0f, -6.0f, 5.0f };
    snprintf(path->bound_camera_id, sizeof(path->bound_camera_id), "camera_%03zu", index + 1u);
    (void)Layout_SceneAuthoringState_Select(state,
                                            LINE_DRAWING_SCENE_AUTHORING_SELECTION_CAMERA_PATH,
                                            index);
    if (out_index) *out_index = index;
    return true;
}

bool Layout_SceneAuthoringState_AddDefaultMaterial(LineDrawingSceneAuthoringState* state,
                                                   size_t* out_index) {
    LineDrawingSceneMaterial* material = NULL;
    size_t index = 0u;
    if (!state || state->material_count >= LINE_DRAWING_SCENE_AUTHORING_MAX_MATERIALS) return false;
    index = state->material_count++;
    material = &state->materials[index];
    memset(material, 0, sizeof(*material));
    snprintf(material->material_id, sizeof(material->material_id), "mat_%03zu", index + 1u);
    snprintf(material->label, sizeof(material->label), "Material %zu", index + 1u);
    material->rgba[0] = 0.45f;
    material->rgba[1] = 0.62f;
    material->rgba[2] = 0.80f;
    material->rgba[3] = 1.0f;
    (void)Layout_SceneAuthoringState_Select(state,
                                            LINE_DRAWING_SCENE_AUTHORING_SELECTION_MATERIAL,
                                            index);
    if (out_index) *out_index = index;
    return true;
}

bool Layout_SceneAuthoringState_DeleteSelected(LineDrawingSceneAuthoringState* state) {
    char deleted_id[LINE_DRAWING_SCENE_AUTHORING_ID_SIZE];
    size_t index = 0u;
    if (!state || state->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_NONE) {
        return false;
    }
    deleted_id[0] = '\0';
    index = state->selected_index;

    if (state->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT) {
        if (index >= state->light_count) return false;
        ld_scene_authoring_copy_text(deleted_id, sizeof(deleted_id), state->lights[index].light_id);
        for (size_t i = index + 1u; i < state->light_count; ++i) {
            state->lights[i - 1u] = state->lights[i];
        }
        state->light_count--;
        memset(&state->lights[state->light_count], 0, sizeof(state->lights[state->light_count]));
        for (size_t i = 0u; i < state->camera_path_count; ++i) {
            if (strncmp(state->camera_paths[i].bound_light_id,
                        deleted_id,
                        sizeof(state->camera_paths[i].bound_light_id)) == 0) {
                state->camera_paths[i].bound_light_id[0] = '\0';
            }
        }
        Layout_SceneAuthoringState_ClearSelection(state);
        return true;
    }

    if (state->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_CAMERA_PATH) {
        if (index >= state->camera_path_count) return false;
        ld_scene_authoring_copy_text(deleted_id, sizeof(deleted_id), state->camera_paths[index].path_id);
        for (size_t i = index + 1u; i < state->camera_path_count; ++i) {
            state->camera_paths[i - 1u] = state->camera_paths[i];
        }
        state->camera_path_count--;
        memset(&state->camera_paths[state->camera_path_count],
               0,
               sizeof(state->camera_paths[state->camera_path_count]));
        for (size_t i = 0u; i < state->light_count; ++i) {
            if (strncmp(state->lights[i].path_id,
                        deleted_id,
                        sizeof(state->lights[i].path_id)) == 0) {
                state->lights[i].path_id[0] = '\0';
            }
        }
        Layout_SceneAuthoringState_ClearSelection(state);
        return true;
    }

    if (state->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_MATERIAL) {
        if (index >= state->material_count) return false;
        for (size_t i = index + 1u; i < state->material_count; ++i) {
            state->materials[i - 1u] = state->materials[i];
        }
        state->material_count--;
        memset(&state->materials[state->material_count],
               0,
               sizeof(state->materials[state->material_count]));
        Layout_SceneAuthoringState_ClearSelection(state);
        return true;
    }

    return false;
}

bool Layout_SceneAuthoringState_ToggleSelectedLightEnabled(LineDrawingSceneAuthoringState* state) {
    LineDrawingSceneLight* light = NULL;
    if (!state ||
        state->selected_kind != LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT ||
        state->selected_index >= state->light_count) {
        return false;
    }
    light = &state->lights[state->selected_index];
    light->enabled = !light->enabled;
    return true;
}

bool Layout_SceneAuthoringState_CycleSelectedLightKind(LineDrawingSceneAuthoringState* state) {
    LineDrawingSceneLight* light = NULL;
    if (!state ||
        state->selected_kind != LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT ||
        state->selected_index >= state->light_count) {
        return false;
    }
    light = &state->lights[state->selected_index];
    switch (light->kind) {
        case LINE_DRAWING_SCENE_LIGHT_DIRECTIONAL:
            light->kind = LINE_DRAWING_SCENE_LIGHT_POINT;
            break;
        case LINE_DRAWING_SCENE_LIGHT_POINT:
            light->kind = LINE_DRAWING_SCENE_LIGHT_SPOT;
            break;
        case LINE_DRAWING_SCENE_LIGHT_SPOT:
        default:
            light->kind = LINE_DRAWING_SCENE_LIGHT_DIRECTIONAL;
            break;
    }
    return true;
}

bool Layout_SceneAuthoringState_CycleSelectedLightPath(LineDrawingSceneAuthoringState* state) {
    LineDrawingSceneLight* light = NULL;
    size_t next_index = 0u;
    if (!state ||
        state->selected_kind != LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT ||
        state->selected_index >= state->light_count) {
        return false;
    }
    light = &state->lights[state->selected_index];
    if (state->camera_path_count == 0u) {
        light->path_id[0] = '\0';
        return true;
    }
    if (light->path_id[0] != '\0') {
        for (size_t i = 0u; i < state->camera_path_count; ++i) {
            if (strncmp(light->path_id,
                        state->camera_paths[i].path_id,
                        sizeof(light->path_id)) == 0) {
                next_index = i + 1u;
                break;
            }
        }
    }
    if (light->path_id[0] != '\0' && next_index >= state->camera_path_count) {
        light->path_id[0] = '\0';
        return true;
    }
    ld_scene_authoring_copy_text(light->path_id,
                                 sizeof(light->path_id),
                                 state->camera_paths[next_index].path_id);
    return true;
}

bool Layout_SceneAuthoringState_CycleSelectedCameraPathKind(LineDrawingSceneAuthoringState* state) {
    LineDrawingSceneCameraPath* path = NULL;
    if (!state ||
        state->selected_kind != LINE_DRAWING_SCENE_AUTHORING_SELECTION_CAMERA_PATH ||
        state->selected_index >= state->camera_path_count) {
        return false;
    }
    path = &state->camera_paths[state->selected_index];
    if (strncmp(path->path_kind, "bezier", sizeof(path->path_kind)) == 0) {
        ld_scene_authoring_copy_text(path->path_kind, sizeof(path->path_kind), "linear");
    } else {
        ld_scene_authoring_copy_text(path->path_kind, sizeof(path->path_kind), "bezier");
    }
    return true;
}

bool Layout_SceneAuthoringState_CycleSelectedMaterialColor(LineDrawingSceneAuthoringState* state) {
    LineDrawingSceneMaterial* material = NULL;
    size_t palette_count =
        sizeof(k_scene_authoring_material_palette) / sizeof(k_scene_authoring_material_palette[0]);
    size_t next_index = 0u;
    if (!state ||
        state->selected_kind != LINE_DRAWING_SCENE_AUTHORING_SELECTION_MATERIAL ||
        state->selected_index >= state->material_count ||
        palette_count == 0u) {
        return false;
    }
    material = &state->materials[state->selected_index];
    for (size_t i = 0u; i < palette_count; ++i) {
        if (ld_scene_authoring_same_rgba(material->rgba,
                                         k_scene_authoring_material_palette[i].rgba)) {
            next_index = (i + 1u) % palette_count;
            break;
        }
    }
    for (int c = 0; c < 4; ++c) {
        material->rgba[c] = k_scene_authoring_material_palette[next_index].rgba[c];
    }
    return true;
}

LineDrawingSceneCameraPath* Layout_SceneAuthoringState_FindCameraPathById(
    LineDrawingSceneAuthoringState* state,
    const char* path_id) {
    if (!state || !path_id || path_id[0] == '\0') return NULL;
    for (size_t i = 0u; i < state->camera_path_count; ++i) {
        if (strncmp(state->camera_paths[i].path_id,
                    path_id,
                    sizeof(state->camera_paths[i].path_id)) == 0) {
            return &state->camera_paths[i];
        }
    }
    return NULL;
}

const LineDrawingSceneCameraPath* Layout_SceneAuthoringState_FindCameraPathByIdConst(
    const LineDrawingSceneAuthoringState* state,
    const char* path_id) {
    if (!state || !path_id || path_id[0] == '\0') return NULL;
    for (size_t i = 0u; i < state->camera_path_count; ++i) {
        if (strncmp(state->camera_paths[i].path_id,
                    path_id,
                    sizeof(state->camera_paths[i].path_id)) == 0) {
            return &state->camera_paths[i];
        }
    }
    return NULL;
}

bool Layout_SceneAuthoringState_SetCameraPathControlPoint(
    LineDrawingSceneAuthoringState* state,
    size_t path_index,
    size_t control_index,
    Vec3 point) {
    LineDrawingSceneCameraPath* path = NULL;
    if (!state || path_index >= state->camera_path_count) return false;
    path = &state->camera_paths[path_index];
    if (control_index >= path->control_point_count ||
        control_index >= (sizeof(path->control_points) / sizeof(path->control_points[0]))) {
        return false;
    }
    path->control_points[control_index] = point;
    return true;
}

bool Layout_SceneAuthoringState_InsertCameraPathControlPoint(
    LineDrawingSceneAuthoringState* state,
    size_t path_index,
    size_t insert_index,
    Vec3 point) {
    LineDrawingSceneCameraPath* path = NULL;
    size_t capacity = 0u;
    if (!state || path_index >= state->camera_path_count) return false;
    path = &state->camera_paths[path_index];
    capacity = sizeof(path->control_points) / sizeof(path->control_points[0]);
    if (path->control_point_count >= capacity) return false;
    if (insert_index > path->control_point_count) insert_index = path->control_point_count;
    for (size_t i = path->control_point_count; i > insert_index; --i) {
        path->control_points[i] = path->control_points[i - 1u];
    }
    path->control_points[insert_index] = point;
    path->control_point_count++;
    return true;
}

bool Layout_SceneAuthoringState_DeleteCameraPathControlPoint(
    LineDrawingSceneAuthoringState* state,
    size_t path_index,
    size_t control_index) {
    LineDrawingSceneCameraPath* path = NULL;
    if (!state || path_index >= state->camera_path_count) return false;
    path = &state->camera_paths[path_index];
    if (path->control_point_count <= 2u ||
        control_index >= path->control_point_count) {
        return false;
    }
    for (size_t i = control_index + 1u; i < path->control_point_count; ++i) {
        path->control_points[i - 1u] = path->control_points[i];
    }
    path->control_point_count--;
    return true;
}

bool Layout_SceneAuthoringState_SetLightPosition(LineDrawingSceneAuthoringState* state,
                                                 size_t light_index,
                                                 Vec3 point) {
    if (!state || light_index >= state->light_count) return false;
    state->lights[light_index].position = point;
    return true;
}

const char* Layout_SceneLightKind_Label(LineDrawingSceneLightKind kind) {
    switch (kind) {
        case LINE_DRAWING_SCENE_LIGHT_POINT: return "Point";
        case LINE_DRAWING_SCENE_LIGHT_SPOT: return "Spot";
        case LINE_DRAWING_SCENE_LIGHT_DIRECTIONAL:
        default: return "Directional";
    }
}
