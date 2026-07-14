#include "Layout/scene/layout_scene_authoring.h"
#include "Layout/scene/layout_scene_camera_authoring.h"
#include "Layout/scene/layout_scene_light_authoring.h"

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

static void ld_scene_authoring_init_path_modes(LineDrawingScenePath* path) {
    if (!path) return;
    for (size_t i = 0u; i < LINE_DRAWING_SCENE_AUTHORING_MAX_PATH_ANCHORS; ++i) {
        path->tangent_modes[i] = LINE_DRAWING_SCENE_PATH_TANGENT_SMOOTH;
    }
    path->playback_mode = LINE_DRAWING_SCENE_PATH_PLAYBACK_ONCE;
    path->duration_seconds = 5.0f;
    path->normalized_distance = 0.0f;
    path->playing = false;
}

void Layout_SceneAuthoringState_ClearSelection(LineDrawingSceneAuthoringState* state) {
    if (!state) return;
    state->selected_kind = LINE_DRAWING_SCENE_AUTHORING_SELECTION_NONE;
    state->selected_index = 0u;
}

void Layout_SceneAuthoringState_Init(LineDrawingSceneAuthoringState* state) {
    LineDrawingSceneLight* light = NULL;
    LineDrawingScenePath* path = NULL;
    LineDrawingSceneMaterial* material = NULL;
    if (!state) return;
    memset(state, 0, sizeof(*state));

    state->light_count = 1u;
    light = &state->lights[0];
    Layout_SceneLight_SetDefaults(light, "light_key", "Key Light");
    light->kind = LINE_DRAWING_SCENE_LIGHT_DIRECTIONAL;
    light->position = (Vec3){ 0.0f, -6.0f, 8.0f };
    light->direction = (Vec3){ 0.2f, 0.4f, -1.0f };
    light->aim_target = Vec3_Add(light->position, Vec3_Scale(Vec3_Normalize(light->direction), 4.0f));
    ld_scene_authoring_copy_text(light->path_id, sizeof(light->path_id), "path_light_key");
    light->enabled = true;

    state->path_count = 2u;
    path = &state->paths[0];
    ld_scene_authoring_copy_text(path->path_id, sizeof(path->path_id), "path_camera_main");
    ld_scene_authoring_copy_text(path->label, sizeof(path->label), "Main Camera Path");
    path->role = LINE_DRAWING_SCENE_PATH_ROLE_CAMERA;
    ld_scene_authoring_copy_text(path->curve_type, sizeof(path->curve_type), "bezier");
    path->control_point_count = 4u;
    path->control_points[0] = (Vec3){ -8.0f, -8.0f, 6.0f };
    path->control_points[1] = (Vec3){ -3.0f, -11.0f, 9.0f };
    path->control_points[2] = (Vec3){ 3.0f, -11.0f, 9.0f };
    path->control_points[3] = (Vec3){ 8.0f, -8.0f, 6.0f };
    ld_scene_authoring_init_path_modes(path);
    ld_scene_authoring_copy_text(path->bound_camera_id, sizeof(path->bound_camera_id), "camera_main");

    state->camera_count = 1u;
    Layout_SceneCamera_SetDefaults(&state->cameras[0],
                                   "camera_main",
                                   "Main Camera",
                                   path->path_id);
    state->cameras[0].position = path->control_points[0];

    path = &state->paths[1];
    ld_scene_authoring_copy_text(path->path_id, sizeof(path->path_id), "path_light_key");
    ld_scene_authoring_copy_text(path->label, sizeof(path->label), "Key Light Path");
    path->role = LINE_DRAWING_SCENE_PATH_ROLE_LIGHT;
    ld_scene_authoring_copy_text(path->curve_type, sizeof(path->curve_type), "bezier");
    path->control_point_count = 4u;
    path->control_points[0] = (Vec3){ -4.0f, -6.0f, 7.0f };
    path->control_points[1] = (Vec3){ -1.5f, -8.0f, 10.0f };
    path->control_points[2] = (Vec3){ 1.5f, -8.0f, 10.0f };
    path->control_points[3] = (Vec3){ 4.0f, -6.0f, 7.0f };
    ld_scene_authoring_init_path_modes(path);
    ld_scene_authoring_copy_text(path->bound_light_id, sizeof(path->bound_light_id), "light_key");

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
        case LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH:
            if (index >= state->path_count) return false;
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
    {
        char light_id[LINE_DRAWING_SCENE_AUTHORING_ID_SIZE];
        char label[LINE_DRAWING_SCENE_AUTHORING_LABEL_SIZE];
        snprintf(light_id, sizeof(light_id), "light_%03zu", index + 1u);
        snprintf(label, sizeof(label), "Light %zu", index + 1u);
        Layout_SceneLight_SetDefaults(light, light_id, label);
    }
    (void)Layout_SceneAuthoringState_Select(state,
                                            LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT,
                                            index);
    if (out_index) *out_index = index;
    return true;
}

bool Layout_SceneAuthoringState_AddDefaultPath(LineDrawingSceneAuthoringState* state,
                                               LineDrawingScenePathRole role,
                                               size_t* out_index) {
    LineDrawingScenePath* path = NULL;
    size_t index = 0u;
    if (!state ||
        state->path_count >= LINE_DRAWING_SCENE_AUTHORING_MAX_PATHS ||
        (role == LINE_DRAWING_SCENE_PATH_ROLE_CAMERA &&
         state->camera_count >= LINE_DRAWING_SCENE_AUTHORING_MAX_CAMERAS)) {
        return false;
    }
    index = state->path_count++;
    path = &state->paths[index];
    memset(path, 0, sizeof(*path));
    path->role = role;
    snprintf(path->path_id,
             sizeof(path->path_id),
             "path_%s_%03zu",
             Layout_ScenePathRole_Name(role),
             index + 1u);
    snprintf(path->label,
             sizeof(path->label),
             "%s Path %zu",
             role == LINE_DRAWING_SCENE_PATH_ROLE_CAMERA ? "Camera" :
             role == LINE_DRAWING_SCENE_PATH_ROLE_LIGHT ? "Light" : "Generic",
             index + 1u);
    ld_scene_authoring_copy_text(path->curve_type, sizeof(path->curve_type), "bezier");
    path->control_point_count = 4u;
    path->control_points[0] = (Vec3){ -6.0f, -6.0f, 5.0f };
    path->control_points[1] = (Vec3){ -2.0f, -9.0f, 8.0f };
    path->control_points[2] = (Vec3){ 2.0f, -9.0f, 8.0f };
    path->control_points[3] = (Vec3){ 6.0f, -6.0f, 5.0f };
    ld_scene_authoring_init_path_modes(path);
    if (role == LINE_DRAWING_SCENE_PATH_ROLE_CAMERA) {
        snprintf(path->bound_camera_id, sizeof(path->bound_camera_id), "camera_%03zu", index + 1u);
        if (state->camera_count < LINE_DRAWING_SCENE_AUTHORING_MAX_CAMERAS) {
            LineDrawingSceneCamera* camera = &state->cameras[state->camera_count++];
            Layout_SceneCamera_SetDefaults(camera,
                                           path->bound_camera_id,
                                           path->label,
                                           path->path_id);
            camera->position = path->control_points[0];
        }
    }
    (void)Layout_SceneAuthoringState_Select(state,
                                            LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH,
                                            index);
    if (out_index) *out_index = index;
    return true;
}

bool Layout_SceneAuthoringState_AddDefaultCameraPath(LineDrawingSceneAuthoringState* state,
                                                     size_t* out_index) {
    return Layout_SceneAuthoringState_AddDefaultPath(state,
                                                     LINE_DRAWING_SCENE_PATH_ROLE_CAMERA,
                                                     out_index);
}

bool Layout_SceneAuthoringState_AddDefaultLightPath(LineDrawingSceneAuthoringState* state,
                                                    size_t* out_index) {
    size_t index = 0u;
    size_t light_index = (size_t)-1;
    if (state && state->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT &&
        state->selected_index < state->light_count) {
        light_index = state->selected_index;
    }
    if (!Layout_SceneAuthoringState_AddDefaultPath(state,
                                                   LINE_DRAWING_SCENE_PATH_ROLE_LIGHT,
                                                   &index)) {
        return false;
    }
    if (light_index != (size_t)-1) {
        LineDrawingScenePath* path = &state->paths[index];
        LineDrawingSceneLight* light = &state->lights[light_index];
        for (size_t i = 0u; i < state->path_count; ++i) {
            if (strcmp(state->paths[i].bound_light_id, light->light_id) == 0) {
                state->paths[i].bound_light_id[0] = '\0';
            }
        }
        ld_scene_authoring_copy_text(path->bound_light_id,
                                     sizeof(path->bound_light_id),
                                     light->light_id);
        ld_scene_authoring_copy_text(light->path_id, sizeof(light->path_id), path->path_id);
        light->position_mode = LINE_DRAWING_SCENE_LIGHT_POSITION_PATH_START;
    }
    if (out_index) *out_index = index;
    return true;
}

bool Layout_SceneAuthoringState_AddDefaultGenericPath(LineDrawingSceneAuthoringState* state,
                                                      size_t* out_index) {
    return Layout_SceneAuthoringState_AddDefaultPath(state,
                                                     LINE_DRAWING_SCENE_PATH_ROLE_GENERIC,
                                                     out_index);
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
        for (size_t i = 0u; i < state->path_count; ++i) {
            if (strncmp(state->paths[i].bound_light_id,
                        deleted_id,
                        sizeof(state->paths[i].bound_light_id)) == 0) {
                state->paths[i].bound_light_id[0] = '\0';
            }
        }
        Layout_SceneAuthoringState_ClearSelection(state);
        return true;
    }

    if (state->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH) {
        if (index >= state->path_count) return false;
        ld_scene_authoring_copy_text(deleted_id, sizeof(deleted_id), state->paths[index].path_id);
        for (size_t i = index + 1u; i < state->path_count; ++i) {
            state->paths[i - 1u] = state->paths[i];
        }
        state->path_count--;
        memset(&state->paths[state->path_count],
               0,
               sizeof(state->paths[state->path_count]));
        for (size_t i = 0u; i < state->light_count; ++i) {
            if (strncmp(state->lights[i].path_id,
                        deleted_id,
                        sizeof(state->lights[i].path_id)) == 0) {
                state->lights[i].path_id[0] = '\0';
            }
        }
        for (size_t i = 0u; i < state->camera_count;) {
            if (strncmp(state->cameras[i].path_id,
                        deleted_id,
                        sizeof(state->cameras[i].path_id)) == 0) {
                for (size_t j = i + 1u; j < state->camera_count; ++j) {
                    state->cameras[j - 1u] = state->cameras[j];
                }
                state->camera_count--;
                memset(&state->cameras[state->camera_count],
                       0,
                       sizeof(state->cameras[state->camera_count]));
                continue;
            }
            ++i;
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
            light->kind = LINE_DRAWING_SCENE_LIGHT_AREA;
            break;
        case LINE_DRAWING_SCENE_LIGHT_AREA:
        default:
            light->kind = LINE_DRAWING_SCENE_LIGHT_DIRECTIONAL;
            break;
    }
    return true;
}

bool Layout_SceneAuthoringState_CycleSelectedLightPath(LineDrawingSceneAuthoringState* state) {
    LineDrawingSceneLight* light = NULL;
    size_t current_index = (size_t)-1;
    size_t next_index = (size_t)-1;
    if (!state ||
        state->selected_kind != LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT ||
        state->selected_index >= state->light_count) {
        return false;
    }
    light = &state->lights[state->selected_index];
    for (size_t i = 0u; i < state->path_count; ++i) {
        if (strcmp(state->paths[i].bound_light_id, light->light_id) == 0) {
            state->paths[i].bound_light_id[0] = '\0';
        }
        if (light->path_id[0] && strcmp(light->path_id, state->paths[i].path_id) == 0) {
            current_index = i;
        }
    }

    if (current_index != (size_t)-1) {
        light->path_id[0] = '\0';
        light->position_mode = LINE_DRAWING_SCENE_LIGHT_POSITION_INDEPENDENT;
        return true;
    }

    for (size_t i = 0u; i < state->path_count; ++i) {
        if (state->paths[i].role == LINE_DRAWING_SCENE_PATH_ROLE_LIGHT &&
            state->paths[i].bound_light_id[0] == '\0') {
            next_index = i;
            break;
        }
    }
    if (next_index == (size_t)-1) return true;
    ld_scene_authoring_copy_text(light->path_id,
                                 sizeof(light->path_id),
                                 state->paths[next_index].path_id);
    ld_scene_authoring_copy_text(state->paths[next_index].bound_light_id,
                                 sizeof(state->paths[next_index].bound_light_id),
                                 light->light_id);
    return true;
}

bool Layout_SceneAuthoringState_CycleSelectedPathCurveType(LineDrawingSceneAuthoringState* state) {
    LineDrawingScenePath* path = NULL;
    if (!state ||
        state->selected_kind != LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH ||
        state->selected_index >= state->path_count) {
        return false;
    }
    path = &state->paths[state->selected_index];
    if (strncmp(path->curve_type, "bezier", sizeof(path->curve_type)) == 0) {
        ld_scene_authoring_copy_text(path->curve_type, sizeof(path->curve_type), "linear");
    } else {
        ld_scene_authoring_copy_text(path->curve_type, sizeof(path->curve_type), "bezier");
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

LineDrawingScenePath* Layout_SceneAuthoringState_FindPathById(
    LineDrawingSceneAuthoringState* state,
    const char* path_id) {
    if (!state || !path_id || path_id[0] == '\0') return NULL;
    for (size_t i = 0u; i < state->path_count; ++i) {
        if (strncmp(state->paths[i].path_id,
                    path_id,
                    sizeof(state->paths[i].path_id)) == 0) {
            return &state->paths[i];
        }
    }
    return NULL;
}

const LineDrawingScenePath* Layout_SceneAuthoringState_FindPathByIdConst(
    const LineDrawingSceneAuthoringState* state,
    const char* path_id) {
    if (!state || !path_id || path_id[0] == '\0') return NULL;
    for (size_t i = 0u; i < state->path_count; ++i) {
        if (strncmp(state->paths[i].path_id,
                    path_id,
                    sizeof(state->paths[i].path_id)) == 0) {
            return &state->paths[i];
        }
    }
    return NULL;
}

bool Layout_SceneAuthoringState_SetPathControlPoint(
    LineDrawingSceneAuthoringState* state,
    size_t path_index,
    size_t control_index,
    Vec3 point) {
    LineDrawingScenePath* path = NULL;
    if (!state || path_index >= state->path_count) return false;
    path = &state->paths[path_index];
    if (control_index >= path->control_point_count ||
        control_index >= (sizeof(path->control_points) / sizeof(path->control_points[0]))) {
        return false;
    }
    path->control_points[control_index] = point;
    return true;
}

bool Layout_SceneAuthoringState_InsertPathControlPoint(
    LineDrawingSceneAuthoringState* state,
    size_t path_index,
    size_t insert_index,
    Vec3 point) {
    LineDrawingScenePath* path = NULL;
    size_t capacity = 0u;
    if (!state || path_index >= state->path_count) return false;
    path = &state->paths[path_index];
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

bool Layout_SceneAuthoringState_DeletePathControlPoint(
    LineDrawingSceneAuthoringState* state,
    size_t path_index,
    size_t control_index) {
    LineDrawingScenePath* path = NULL;
    if (!state || path_index >= state->path_count) return false;
    path = &state->paths[path_index];
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
        case LINE_DRAWING_SCENE_LIGHT_AREA: return "Area";
        case LINE_DRAWING_SCENE_LIGHT_DIRECTIONAL:
        default: return "Directional";
    }
}

const char* Layout_ScenePathRole_Name(LineDrawingScenePathRole role) {
    switch (role) {
        case LINE_DRAWING_SCENE_PATH_ROLE_CAMERA: return "camera";
        case LINE_DRAWING_SCENE_PATH_ROLE_LIGHT: return "light";
        case LINE_DRAWING_SCENE_PATH_ROLE_GENERIC:
        default: return "generic";
    }
}

bool Layout_ScenePathRole_FromName(const char* name, LineDrawingScenePathRole* out_role) {
    if (out_role) *out_role = LINE_DRAWING_SCENE_PATH_ROLE_GENERIC;
    if (!name || !out_role) return false;
    if (strcmp(name, "camera") == 0) {
        *out_role = LINE_DRAWING_SCENE_PATH_ROLE_CAMERA;
        return true;
    }
    if (strcmp(name, "light") == 0) {
        *out_role = LINE_DRAWING_SCENE_PATH_ROLE_LIGHT;
        return true;
    }
    if (strcmp(name, "generic") == 0) {
        *out_role = LINE_DRAWING_SCENE_PATH_ROLE_GENERIC;
        return true;
    }
    return false;
}

const char* Layout_ScenePathTangentMode_Name(LineDrawingScenePathTangentMode mode) {
    switch (mode) {
        case LINE_DRAWING_SCENE_PATH_TANGENT_LINKED: return "linked";
        case LINE_DRAWING_SCENE_PATH_TANGENT_BROKEN: return "broken";
        case LINE_DRAWING_SCENE_PATH_TANGENT_AUTOMATIC: return "automatic";
        case LINE_DRAWING_SCENE_PATH_TANGENT_SMOOTH: return "smooth";
        case LINE_DRAWING_SCENE_PATH_TANGENT_CORNER: return "corner";
        default: return "smooth";
    }
}

bool Layout_ScenePathTangentMode_FromName(const char* name,
                                         LineDrawingScenePathTangentMode* out_mode) {
    LineDrawingScenePathTangentMode mode = LINE_DRAWING_SCENE_PATH_TANGENT_SMOOTH;
    if (!name || !out_mode) return false;
    if (strcmp(name, "linked") == 0) mode = LINE_DRAWING_SCENE_PATH_TANGENT_LINKED;
    else if (strcmp(name, "broken") == 0) mode = LINE_DRAWING_SCENE_PATH_TANGENT_BROKEN;
    else if (strcmp(name, "automatic") == 0) mode = LINE_DRAWING_SCENE_PATH_TANGENT_AUTOMATIC;
    else if (strcmp(name, "smooth") == 0) mode = LINE_DRAWING_SCENE_PATH_TANGENT_SMOOTH;
    else if (strcmp(name, "corner") == 0) mode = LINE_DRAWING_SCENE_PATH_TANGENT_CORNER;
    else return false;
    *out_mode = mode;
    return true;
}

const char* Layout_ScenePathPlaybackMode_Name(LineDrawingScenePathPlaybackMode mode) {
    return mode == LINE_DRAWING_SCENE_PATH_PLAYBACK_LOOP ? "loop" : "once";
}

bool Layout_ScenePathPlaybackMode_FromName(const char* name,
                                           LineDrawingScenePathPlaybackMode* out_mode) {
    if (!name || !out_mode) return false;
    if (strcmp(name, "once") == 0) {
        *out_mode = LINE_DRAWING_SCENE_PATH_PLAYBACK_ONCE;
        return true;
    }
    if (strcmp(name, "loop") == 0) {
        *out_mode = LINE_DRAWING_SCENE_PATH_PLAYBACK_LOOP;
        return true;
    }
    return false;
}
