#pragma once

#include <stddef.h>
#include <stdbool.h>

#include "Math/math_util.h"

#define LINE_DRAWING_SCENE_AUTHORING_MAX_LIGHTS 8
#define LINE_DRAWING_SCENE_AUTHORING_MAX_CAMERA_PATHS 8
#define LINE_DRAWING_SCENE_AUTHORING_MAX_MATERIALS 16
#define LINE_DRAWING_SCENE_AUTHORING_ID_SIZE 64
#define LINE_DRAWING_SCENE_AUTHORING_LABEL_SIZE 64

typedef enum {
    LINE_DRAWING_SCENE_AUTHORING_SELECTION_NONE = 0,
    LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT = 1,
    LINE_DRAWING_SCENE_AUTHORING_SELECTION_CAMERA_PATH = 2,
    LINE_DRAWING_SCENE_AUTHORING_SELECTION_MATERIAL = 3
} LineDrawingSceneAuthoringSelectionKind;

typedef enum {
    LINE_DRAWING_SCENE_LIGHT_DIRECTIONAL = 0,
    LINE_DRAWING_SCENE_LIGHT_POINT = 1,
    LINE_DRAWING_SCENE_LIGHT_SPOT = 2
} LineDrawingSceneLightKind;

typedef struct {
    char light_id[LINE_DRAWING_SCENE_AUTHORING_ID_SIZE];
    char label[LINE_DRAWING_SCENE_AUTHORING_LABEL_SIZE];
    LineDrawingSceneLightKind kind;
    Vec3 position;
    Vec3 direction;
    char path_id[LINE_DRAWING_SCENE_AUTHORING_ID_SIZE];
    bool enabled;
} LineDrawingSceneLight;

typedef struct {
    char path_id[LINE_DRAWING_SCENE_AUTHORING_ID_SIZE];
    char label[LINE_DRAWING_SCENE_AUTHORING_LABEL_SIZE];
    char path_kind[LINE_DRAWING_SCENE_AUTHORING_ID_SIZE];
    Vec3 control_points[4];
    size_t control_point_count;
    char bound_light_id[LINE_DRAWING_SCENE_AUTHORING_ID_SIZE];
    char bound_camera_id[LINE_DRAWING_SCENE_AUTHORING_ID_SIZE];
} LineDrawingSceneCameraPath;

typedef struct {
    char material_id[LINE_DRAWING_SCENE_AUTHORING_ID_SIZE];
    char label[LINE_DRAWING_SCENE_AUTHORING_LABEL_SIZE];
    float rgba[4];
} LineDrawingSceneMaterial;

typedef struct {
    LineDrawingSceneLight lights[LINE_DRAWING_SCENE_AUTHORING_MAX_LIGHTS];
    size_t light_count;
    LineDrawingSceneCameraPath camera_paths[LINE_DRAWING_SCENE_AUTHORING_MAX_CAMERA_PATHS];
    size_t camera_path_count;
    LineDrawingSceneMaterial materials[LINE_DRAWING_SCENE_AUTHORING_MAX_MATERIALS];
    size_t material_count;
    LineDrawingSceneAuthoringSelectionKind selected_kind;
    size_t selected_index;
} LineDrawingSceneAuthoringState;

void Layout_SceneAuthoringState_Init(LineDrawingSceneAuthoringState* state);
void Layout_SceneAuthoringState_ClearSelection(LineDrawingSceneAuthoringState* state);
bool Layout_SceneAuthoringState_Select(LineDrawingSceneAuthoringState* state,
                                       LineDrawingSceneAuthoringSelectionKind kind,
                                       size_t index);
bool Layout_SceneAuthoringState_AddDefaultLight(LineDrawingSceneAuthoringState* state,
                                                size_t* out_index);
bool Layout_SceneAuthoringState_AddDefaultCameraPath(LineDrawingSceneAuthoringState* state,
                                                     size_t* out_index);
bool Layout_SceneAuthoringState_AddDefaultMaterial(LineDrawingSceneAuthoringState* state,
                                                   size_t* out_index);
bool Layout_SceneAuthoringState_DeleteSelected(LineDrawingSceneAuthoringState* state);
bool Layout_SceneAuthoringState_ToggleSelectedLightEnabled(LineDrawingSceneAuthoringState* state);
bool Layout_SceneAuthoringState_CycleSelectedLightKind(LineDrawingSceneAuthoringState* state);
bool Layout_SceneAuthoringState_CycleSelectedLightPath(LineDrawingSceneAuthoringState* state);
bool Layout_SceneAuthoringState_CycleSelectedCameraPathKind(LineDrawingSceneAuthoringState* state);
bool Layout_SceneAuthoringState_CycleSelectedMaterialColor(LineDrawingSceneAuthoringState* state);
LineDrawingSceneCameraPath* Layout_SceneAuthoringState_FindCameraPathById(
    LineDrawingSceneAuthoringState* state,
    const char* path_id);
const LineDrawingSceneCameraPath* Layout_SceneAuthoringState_FindCameraPathByIdConst(
    const LineDrawingSceneAuthoringState* state,
    const char* path_id);
bool Layout_SceneAuthoringState_SetCameraPathControlPoint(
    LineDrawingSceneAuthoringState* state,
    size_t path_index,
    size_t control_index,
    Vec3 point);
bool Layout_SceneAuthoringState_InsertCameraPathControlPoint(
    LineDrawingSceneAuthoringState* state,
    size_t path_index,
    size_t insert_index,
    Vec3 point);
bool Layout_SceneAuthoringState_DeleteCameraPathControlPoint(
    LineDrawingSceneAuthoringState* state,
    size_t path_index,
    size_t control_index);
bool Layout_SceneAuthoringState_SetLightPosition(LineDrawingSceneAuthoringState* state,
                                                 size_t light_index,
                                                 Vec3 point);
const char* Layout_SceneLightKind_Label(LineDrawingSceneLightKind kind);
