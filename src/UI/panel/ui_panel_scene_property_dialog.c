#include "UI/ui_panel.h"

#include "Core/global_state.h"
#include "Editor/editor.h"
#include "Layout/scene/layout_scene_camera_authoring.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static LineDrawingSceneLight* UIPanelSceneProperty_SelectedLight(LineDrawingSceneAuthoringState* authoring) {
    if (!authoring || authoring->selected_kind != LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT ||
        authoring->selected_index >= authoring->light_count) return NULL;
    return &authoring->lights[authoring->selected_index];
}

static LineDrawingScenePath* UIPanelSceneProperty_SelectedPath(LineDrawingSceneAuthoringState* authoring) {
    if (!authoring || authoring->selected_kind != LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH ||
        authoring->selected_index >= authoring->path_count) return NULL;
    return &authoring->paths[authoring->selected_index];
}

static LineDrawingSceneCamera* UIPanelSceneProperty_SelectedCamera(LineDrawingSceneAuthoringState* authoring) {
    LineDrawingScenePath* path = UIPanelSceneProperty_SelectedPath(authoring);
    return path ? Layout_SceneAuthoringState_FindCameraForPath(authoring, path) : NULL;
}

static bool UIPanelSceneProperty_ParseFloats(const char* text, float* values, int count) {
    char normalized[160] = {0};
    char trailing = '\0';
    int matched = 0;
    if (!text || !values || count < 1 || count > 3) return false;
    snprintf(normalized, sizeof(normalized), "%s", text);
    for (size_t i = 0u; normalized[i]; ++i) {
        if (normalized[i] == ',') normalized[i] = ' ';
    }
    if (count == 1) matched = sscanf(normalized, " %f %c", &values[0], &trailing);
    if (count == 2) matched = sscanf(normalized, " %f %f %c", &values[0], &values[1], &trailing);
    if (count == 3) matched = sscanf(normalized, " %f %f %f %c", &values[0], &values[1], &values[2], &trailing);
    if (matched != count) return false;
    for (int i = 0; i < count; ++i) {
        if (!isfinite(values[i])) return false;
    }
    return true;
}

static void UIPanelSceneProperty_SetBuffer(UIPanelState* ui, const char* value) {
    if (!ui) return;
    snprintf(ui->scenePropertyDialog.buffer, sizeof(ui->scenePropertyDialog.buffer), "%s", value ? value : "");
    ui->scenePropertyDialog.length = strlen(ui->scenePropertyDialog.buffer);
    ui->scenePropertyDialog.cursor = ui->scenePropertyDialog.length;
    ui->scenePropertyDialog.validationMessage[0] = '\0';
}

const char* UIPanel_ScenePropertyDialogTitle(UIScenePropertyDialogTarget target) {
    switch (target) {
        case UI_SCENE_PROPERTY_DIALOG_LABEL: return "Rename Selection";
        case UI_SCENE_PROPERTY_DIALOG_CAMERA_ROLL: return "Camera Roll";
        case UI_SCENE_PROPERTY_DIALOG_CAMERA_FOV: return "Camera Vertical FOV";
        case UI_SCENE_PROPERTY_DIALOG_CAMERA_CLIP: return "Camera Clip Range";
        case UI_SCENE_PROPERTY_DIALOG_PATH_SCRUB: return "Path Scrub";
        case UI_SCENE_PROPERTY_DIALOG_PATH_DURATION: return "Path Duration";
        case UI_SCENE_PROPERTY_DIALOG_LIGHT_PATH: return "Light Path Binding";
        case UI_SCENE_PROPERTY_DIALOG_LIGHT_COLOR: return "Light RGB";
        case UI_SCENE_PROPERTY_DIALOG_LIGHT_INTENSITY: return "Light Intensity";
        case UI_SCENE_PROPERTY_DIALOG_LIGHT_SIZE: return "Light Radius / Area Size";
        case UI_SCENE_PROPERTY_DIALOG_LIGHT_CONE: return "Spot Cone";
        case UI_SCENE_PROPERTY_DIALOG_NONE:
        default: return "Scene Property";
    }
}

const char* UIPanel_ScenePropertyDialogHint(UIScenePropertyDialogTarget target) {
    switch (target) {
        case UI_SCENE_PROPERTY_DIALOG_LABEL: return "Enter a non-empty display label. Stable ids do not change.";
        case UI_SCENE_PROPERTY_DIALOG_CAMERA_ROLL: return "Enter roll in degrees.";
        case UI_SCENE_PROPERTY_DIALOG_CAMERA_FOV: return "Enter a vertical FOV from 1 to 179 degrees.";
        case UI_SCENE_PROPERTY_DIALOG_CAMERA_CLIP: return "Enter near, far with 0 < near < far.";
        case UI_SCENE_PROPERTY_DIALOG_PATH_SCRUB: return "Enter playback position from 0 to 100 percent.";
        case UI_SCENE_PROPERTY_DIALOG_PATH_DURATION: return "Enter a positive duration in seconds.";
        case UI_SCENE_PROPERTY_DIALOG_LIGHT_PATH: return "Enter an exact path id, or none to unbind.";
        case UI_SCENE_PROPERTY_DIALOG_LIGHT_COLOR: return "Enter red, green, blue values from 0 to 1.";
        case UI_SCENE_PROPERTY_DIALOG_LIGHT_INTENSITY: return "Enter a non-negative intensity.";
        case UI_SCENE_PROPERTY_DIALOG_LIGHT_SIZE: return "Enter radius, or width height for an area light.";
        case UI_SCENE_PROPERTY_DIALOG_LIGHT_CONE: return "Enter inner, outer degrees with inner <= outer < 180.";
        case UI_SCENE_PROPERTY_DIALOG_NONE:
        default: return "Enter applies; Esc cancels.";
    }
}

bool UIPanel_BeginScenePropertyDialog(UIScenePropertyDialogTarget target) {
    UIPanelState* ui = UIPanel_Get();
    GlobalState* state = Global_Get();
    LineDrawingSceneAuthoringState* authoring = state ? &state->layout.sceneAuthoring : NULL;
    LineDrawingSceneLight* light = UIPanelSceneProperty_SelectedLight(authoring);
    LineDrawingScenePath* path = UIPanelSceneProperty_SelectedPath(authoring);
    LineDrawingSceneCamera* camera = UIPanelSceneProperty_SelectedCamera(authoring);
    char initial[160] = {0};
    if (!ui || !authoring || target == UI_SCENE_PROPERTY_DIALOG_NONE) return false;
    switch (target) {
        case UI_SCENE_PROPERTY_DIALOG_LABEL:
            if (light) snprintf(initial, sizeof(initial), "%s", light->label);
            else if (path) snprintf(initial, sizeof(initial), "%s", path->label);
            else if (authoring->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_MATERIAL &&
                     authoring->selected_index < authoring->material_count) {
                snprintf(initial, sizeof(initial), "%s", authoring->materials[authoring->selected_index].label);
            } else return false;
            break;
        case UI_SCENE_PROPERTY_DIALOG_CAMERA_ROLL:
            if (!camera) return false;
            snprintf(initial, sizeof(initial), "%.3f", camera->roll_degrees);
            break;
        case UI_SCENE_PROPERTY_DIALOG_CAMERA_FOV:
            if (!camera) return false;
            snprintf(initial, sizeof(initial), "%.3f", camera->vertical_fov_degrees);
            break;
        case UI_SCENE_PROPERTY_DIALOG_CAMERA_CLIP:
            if (!camera) return false;
            snprintf(initial, sizeof(initial), "%.4f, %.4f", camera->near_clip, camera->far_clip);
            break;
        case UI_SCENE_PROPERTY_DIALOG_PATH_SCRUB:
            if (!path) return false;
            snprintf(initial, sizeof(initial), "%.2f", path->normalized_distance * 100.0f);
            break;
        case UI_SCENE_PROPERTY_DIALOG_PATH_DURATION:
            if (!path) return false;
            snprintf(initial, sizeof(initial), "%.3f", path->duration_seconds);
            break;
        case UI_SCENE_PROPERTY_DIALOG_LIGHT_PATH:
            if (!light) return false;
            snprintf(initial, sizeof(initial), "%s", light->path_id[0] ? light->path_id : "none");
            break;
        case UI_SCENE_PROPERTY_DIALOG_LIGHT_COLOR:
            if (!light) return false;
            snprintf(initial, sizeof(initial), "%.3f, %.3f, %.3f", light->color_rgb[0], light->color_rgb[1], light->color_rgb[2]);
            break;
        case UI_SCENE_PROPERTY_DIALOG_LIGHT_INTENSITY:
            if (!light) return false;
            snprintf(initial, sizeof(initial), "%.3f", light->intensity);
            break;
        case UI_SCENE_PROPERTY_DIALOG_LIGHT_SIZE:
            if (!light) return false;
            if (light->kind == LINE_DRAWING_SCENE_LIGHT_AREA) {
                snprintf(initial, sizeof(initial), "%.3f, %.3f", light->area_size.x, light->area_size.y);
            } else {
                snprintf(initial, sizeof(initial), "%.3f", light->radius);
            }
            break;
        case UI_SCENE_PROPERTY_DIALOG_LIGHT_CONE:
            if (!light) return false;
            snprintf(initial, sizeof(initial), "%.3f, %.3f", light->inner_cone_degrees, light->outer_cone_degrees);
            break;
        case UI_SCENE_PROPERTY_DIALOG_NONE:
        default:
            return false;
    }
    ui->scenePropertyDialog.active = true;
    ui->scenePropertyDialog.target = target;
    UIPanelSceneProperty_SetBuffer(ui, initial);
    SDL_StartTextInput();
    return true;
}

void UIPanel_CloseScenePropertyDialog(UIPanelState* ui) {
    if (!ui) return;
    ui->scenePropertyDialog.active = false;
    ui->scenePropertyDialog.target = UI_SCENE_PROPERTY_DIALOG_NONE;
    ui->scenePropertyDialog.buffer[0] = '\0';
    ui->scenePropertyDialog.length = 0u;
    ui->scenePropertyDialog.cursor = 0u;
    ui->scenePropertyDialog.validationMessage[0] = '\0';
    SDL_StopTextInput();
}

bool UIPanel_ApplyScenePropertyDialog(UIPanelState* ui) {
    GlobalState* state = Global_Get();
    LineDrawingSceneAuthoringState edited = {0};
    LineDrawingSceneLight* light = NULL;
    LineDrawingScenePath* path = NULL;
    LineDrawingSceneCamera* camera = NULL;
    float values[3] = {0.0f, 0.0f, 0.0f};
    bool valid = false;
    if (!ui || !ui->scenePropertyDialog.active || !state) return false;
    edited = state->layout.sceneAuthoring;
    light = UIPanelSceneProperty_SelectedLight(&edited);
    path = UIPanelSceneProperty_SelectedPath(&edited);
    camera = UIPanelSceneProperty_SelectedCamera(&edited);
    switch (ui->scenePropertyDialog.target) {
        case UI_SCENE_PROPERTY_DIALOG_LABEL:
            if (ui->scenePropertyDialog.buffer[0] == '\0') break;
            if (light) snprintf(light->label, sizeof(light->label), "%s", ui->scenePropertyDialog.buffer);
            else if (path) snprintf(path->label, sizeof(path->label), "%s", ui->scenePropertyDialog.buffer);
            else if (edited.selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_MATERIAL &&
                     edited.selected_index < edited.material_count) {
                snprintf(edited.materials[edited.selected_index].label,
                         sizeof(edited.materials[edited.selected_index].label), "%s",
                         ui->scenePropertyDialog.buffer);
            } else break;
            valid = true;
            break;
        case UI_SCENE_PROPERTY_DIALOG_CAMERA_ROLL:
            valid = camera && UIPanelSceneProperty_ParseFloats(ui->scenePropertyDialog.buffer, values, 1) &&
                    values[0] >= -360.0f && values[0] <= 360.0f;
            if (valid) camera->roll_degrees = values[0];
            break;
        case UI_SCENE_PROPERTY_DIALOG_CAMERA_FOV:
            valid = camera && UIPanelSceneProperty_ParseFloats(ui->scenePropertyDialog.buffer, values, 1) &&
                    values[0] >= 1.0f && values[0] <= 179.0f;
            if (valid) camera->vertical_fov_degrees = values[0];
            break;
        case UI_SCENE_PROPERTY_DIALOG_CAMERA_CLIP:
            valid = camera && UIPanelSceneProperty_ParseFloats(ui->scenePropertyDialog.buffer, values, 2) &&
                    values[0] > 0.0f && values[1] > values[0];
            if (valid) { camera->near_clip = values[0]; camera->far_clip = values[1]; }
            break;
        case UI_SCENE_PROPERTY_DIALOG_PATH_SCRUB:
            valid = path && UIPanelSceneProperty_ParseFloats(ui->scenePropertyDialog.buffer, values, 1) &&
                    values[0] >= 0.0f && values[0] <= 100.0f;
            if (valid) { path->normalized_distance = values[0] / 100.0f; path->playing = false; }
            break;
        case UI_SCENE_PROPERTY_DIALOG_PATH_DURATION:
            valid = path && UIPanelSceneProperty_ParseFloats(ui->scenePropertyDialog.buffer, values, 1) &&
                    values[0] > 0.0f && values[0] <= 86400.0f;
            if (valid) path->duration_seconds = values[0];
            break;
        case UI_SCENE_PROPERTY_DIALOG_LIGHT_PATH:
            if (light) {
                LineDrawingScenePath* target = NULL;
                for (size_t i = 0u; i < edited.path_count; ++i) {
                    if (strcmp(edited.paths[i].bound_light_id, light->light_id) == 0) {
                        edited.paths[i].bound_light_id[0] = '\0';
                    }
                    if (strcmp(edited.paths[i].path_id, ui->scenePropertyDialog.buffer) == 0 &&
                        edited.paths[i].role != LINE_DRAWING_SCENE_PATH_ROLE_CAMERA) target = &edited.paths[i];
                }
                if (strcmp(ui->scenePropertyDialog.buffer, "none") == 0 || ui->scenePropertyDialog.buffer[0] == '\0') {
                    light->path_id[0] = '\0';
                    light->position_mode = LINE_DRAWING_SCENE_LIGHT_POSITION_INDEPENDENT;
                    valid = true;
                } else if (target) {
                    snprintf(light->path_id, sizeof(light->path_id), "%s", target->path_id);
                    snprintf(target->bound_light_id, sizeof(target->bound_light_id), "%s", light->light_id);
                    valid = true;
                }
            }
            break;
        case UI_SCENE_PROPERTY_DIALOG_LIGHT_COLOR:
            valid = light && UIPanelSceneProperty_ParseFloats(ui->scenePropertyDialog.buffer, values, 3) &&
                    values[0] >= 0.0f && values[0] <= 1.0f && values[1] >= 0.0f && values[1] <= 1.0f &&
                    values[2] >= 0.0f && values[2] <= 1.0f;
            if (valid) { light->color_rgb[0] = values[0]; light->color_rgb[1] = values[1]; light->color_rgb[2] = values[2]; }
            break;
        case UI_SCENE_PROPERTY_DIALOG_LIGHT_INTENSITY:
            valid = light && UIPanelSceneProperty_ParseFloats(ui->scenePropertyDialog.buffer, values, 1) && values[0] >= 0.0f;
            if (valid) light->intensity = values[0];
            break;
        case UI_SCENE_PROPERTY_DIALOG_LIGHT_SIZE:
            if (light && light->kind == LINE_DRAWING_SCENE_LIGHT_AREA) {
                valid = UIPanelSceneProperty_ParseFloats(ui->scenePropertyDialog.buffer, values, 2) && values[0] > 0.0f && values[1] > 0.0f;
                if (valid) light->area_size = (Vec2){values[0], values[1]};
            } else {
                valid = light && UIPanelSceneProperty_ParseFloats(ui->scenePropertyDialog.buffer, values, 1) && values[0] > 0.0f;
                if (valid) light->radius = values[0];
            }
            break;
        case UI_SCENE_PROPERTY_DIALOG_LIGHT_CONE:
            valid = light && UIPanelSceneProperty_ParseFloats(ui->scenePropertyDialog.buffer, values, 2) &&
                    values[0] >= 0.0f && values[1] >= values[0] && values[1] < 180.0f;
            if (valid) { light->inner_cone_degrees = values[0]; light->outer_cone_degrees = values[1]; }
            break;
        case UI_SCENE_PROPERTY_DIALOG_NONE:
        default:
            break;
    }
    if (!valid) {
        snprintf(ui->scenePropertyDialog.validationMessage,
                 sizeof(ui->scenePropertyDialog.validationMessage),
                 "Value is invalid for this property.");
        return true;
    }
    Editor_HistoryCapture(&state->editor, &state->layout);
    state->layout.sceneAuthoring = edited;
    Global_FlagLayoutChanged();
    UIPanel_CloseScenePropertyDialog(ui);
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);
    return true;
}

bool UIPanel_IsScenePropertyDialogActive(void) {
    UIPanelState* ui = UIPanel_Get();
    return ui && ui->scenePropertyDialog.active;
}
