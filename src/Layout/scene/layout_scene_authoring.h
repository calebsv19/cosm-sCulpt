#pragma once

#include <stddef.h>
#include <stdbool.h>

#include "Math/math_util.h"

#define LINE_DRAWING_SCENE_AUTHORING_MAX_LIGHTS 8
#define LINE_DRAWING_SCENE_AUTHORING_MAX_CAMERAS 8
#define LINE_DRAWING_SCENE_AUTHORING_MAX_PATHS 8
#define LINE_DRAWING_SCENE_AUTHORING_MAX_PATH_POINTS 16
#define LINE_DRAWING_SCENE_AUTHORING_MAX_PATH_ANCHORS 6
#define LINE_DRAWING_SCENE_AUTHORING_MAX_MATERIALS 16
#define LINE_DRAWING_SCENE_AUTHORING_ID_SIZE 64
#define LINE_DRAWING_SCENE_AUTHORING_LABEL_SIZE 64

typedef enum {
    LINE_DRAWING_SCENE_AUTHORING_SELECTION_NONE = 0,
    LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT = 1,
    LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH = 2,
    LINE_DRAWING_SCENE_AUTHORING_SELECTION_MATERIAL = 3
} LineDrawingSceneAuthoringSelectionKind;

typedef enum {
    LINE_DRAWING_SCENE_LIGHT_DIRECTIONAL = 0,
    LINE_DRAWING_SCENE_LIGHT_POINT = 1,
    LINE_DRAWING_SCENE_LIGHT_SPOT = 2,
    LINE_DRAWING_SCENE_LIGHT_AREA = 3
} LineDrawingSceneLightKind;

/* Chooses whether a light uses its authored position or the first point of its bound path. */
typedef enum {
    LINE_DRAWING_SCENE_LIGHT_POSITION_INDEPENDENT = 0,
    LINE_DRAWING_SCENE_LIGHT_POSITION_PATH_START = 1
} LineDrawingSceneLightPositionMode;

/* Renderer-neutral attenuation intent; renderers retain sampling and transport policy. */
typedef enum {
    LINE_DRAWING_SCENE_LIGHT_FALLOFF_INVERSE_SQUARE = 0,
    LINE_DRAWING_SCENE_LIGHT_FALLOFF_LINEAR = 1,
    LINE_DRAWING_SCENE_LIGHT_FALLOFF_CONSTANT = 2
} LineDrawingSceneLightFalloff;

typedef enum {
    LINE_DRAWING_SCENE_PATH_ROLE_GENERIC = 0,
    LINE_DRAWING_SCENE_PATH_ROLE_CAMERA = 1,
    LINE_DRAWING_SCENE_PATH_ROLE_LIGHT = 2
} LineDrawingScenePathRole;

typedef enum {
    LINE_DRAWING_SCENE_PATH_TANGENT_LINKED = 0,
    LINE_DRAWING_SCENE_PATH_TANGENT_BROKEN = 1,
    LINE_DRAWING_SCENE_PATH_TANGENT_AUTOMATIC = 2,
    LINE_DRAWING_SCENE_PATH_TANGENT_SMOOTH = 3,
    LINE_DRAWING_SCENE_PATH_TANGENT_CORNER = 4
} LineDrawingScenePathTangentMode;

typedef enum {
    LINE_DRAWING_SCENE_PATH_PLAYBACK_ONCE = 0,
    LINE_DRAWING_SCENE_PATH_PLAYBACK_LOOP = 1
} LineDrawingScenePathPlaybackMode;

typedef enum {
    LINE_DRAWING_SCENE_CAMERA_ORIENTATION_PATH_FACING = 0,
    LINE_DRAWING_SCENE_CAMERA_ORIENTATION_LOOK_AT_TARGET = 1,
    LINE_DRAWING_SCENE_CAMERA_ORIENTATION_FIXED = 2,
    LINE_DRAWING_SCENE_CAMERA_ORIENTATION_PER_POINT = 3
} LineDrawingSceneCameraOrientationMode;

typedef struct {
    char light_id[LINE_DRAWING_SCENE_AUTHORING_ID_SIZE];
    char label[LINE_DRAWING_SCENE_AUTHORING_LABEL_SIZE];
    LineDrawingSceneLightKind kind;
    Vec3 position;
    Vec3 direction;
    Vec3 aim_target;
    char path_id[LINE_DRAWING_SCENE_AUTHORING_ID_SIZE];
    bool enabled;
    LineDrawingSceneLightPositionMode position_mode;
    float color_rgb[3];
    float intensity;
    float radius;
    Vec2 area_size;
    float inner_cone_degrees;
    float outer_cone_degrees;
    LineDrawingSceneLightFalloff falloff;
} LineDrawingSceneLight;

typedef struct {
    char path_id[LINE_DRAWING_SCENE_AUTHORING_ID_SIZE];
    char label[LINE_DRAWING_SCENE_AUTHORING_LABEL_SIZE];
    LineDrawingScenePathRole role;
    char curve_type[LINE_DRAWING_SCENE_AUTHORING_ID_SIZE];
    Vec3 control_points[LINE_DRAWING_SCENE_AUTHORING_MAX_PATH_POINTS];
    size_t control_point_count;
    LineDrawingScenePathTangentMode
        tangent_modes[LINE_DRAWING_SCENE_AUTHORING_MAX_PATH_ANCHORS];
    char bound_light_id[LINE_DRAWING_SCENE_AUTHORING_ID_SIZE];
    char bound_camera_id[LINE_DRAWING_SCENE_AUTHORING_ID_SIZE];
    bool closed;
    LineDrawingScenePathPlaybackMode playback_mode;
    float duration_seconds;
    float normalized_distance;
    bool playing;
} LineDrawingScenePath;

typedef struct {
    char camera_id[LINE_DRAWING_SCENE_AUTHORING_ID_SIZE];
    char label[LINE_DRAWING_SCENE_AUTHORING_LABEL_SIZE];
    char path_id[LINE_DRAWING_SCENE_AUTHORING_ID_SIZE];
    Vec3 position;
    Vec3 look_at_target;
    Vec3 fixed_forward;
    LineDrawingSceneCameraOrientationMode orientation_mode;
    float roll_degrees;
    float vertical_fov_degrees;
    float near_clip;
    float far_clip;
} LineDrawingSceneCamera;

typedef struct {
    char material_id[LINE_DRAWING_SCENE_AUTHORING_ID_SIZE];
    char label[LINE_DRAWING_SCENE_AUTHORING_LABEL_SIZE];
    float rgba[4];
} LineDrawingSceneMaterial;

typedef struct {
    LineDrawingSceneLight lights[LINE_DRAWING_SCENE_AUTHORING_MAX_LIGHTS];
    size_t light_count;
    LineDrawingSceneCamera cameras[LINE_DRAWING_SCENE_AUTHORING_MAX_CAMERAS];
    size_t camera_count;
    LineDrawingScenePath paths[LINE_DRAWING_SCENE_AUTHORING_MAX_PATHS];
    size_t path_count;
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
bool Layout_SceneAuthoringState_AddDefaultPath(LineDrawingSceneAuthoringState* state,
                                               LineDrawingScenePathRole role,
                                               size_t* out_index);
bool Layout_SceneAuthoringState_AddDefaultLightPath(LineDrawingSceneAuthoringState* state,
                                                    size_t* out_index);
bool Layout_SceneAuthoringState_AddDefaultGenericPath(LineDrawingSceneAuthoringState* state,
                                                      size_t* out_index);
bool Layout_SceneAuthoringState_AddDefaultMaterial(LineDrawingSceneAuthoringState* state,
                                                   size_t* out_index);
bool Layout_SceneAuthoringState_DeleteSelected(LineDrawingSceneAuthoringState* state);
bool Layout_SceneAuthoringState_ToggleSelectedLightEnabled(LineDrawingSceneAuthoringState* state);
bool Layout_SceneAuthoringState_CycleSelectedLightKind(LineDrawingSceneAuthoringState* state);
bool Layout_SceneAuthoringState_CycleSelectedLightPath(LineDrawingSceneAuthoringState* state);
bool Layout_SceneAuthoringState_CycleSelectedPathCurveType(LineDrawingSceneAuthoringState* state);
bool Layout_SceneAuthoringState_CycleSelectedMaterialColor(LineDrawingSceneAuthoringState* state);
LineDrawingScenePath* Layout_SceneAuthoringState_FindPathById(
    LineDrawingSceneAuthoringState* state,
    const char* path_id);
const LineDrawingScenePath* Layout_SceneAuthoringState_FindPathByIdConst(
    const LineDrawingSceneAuthoringState* state,
    const char* path_id);
bool Layout_SceneAuthoringState_SetPathControlPoint(
    LineDrawingSceneAuthoringState* state,
    size_t path_index,
    size_t control_index,
    Vec3 point);
bool Layout_SceneAuthoringState_InsertPathControlPoint(
    LineDrawingSceneAuthoringState* state,
    size_t path_index,
    size_t insert_index,
    Vec3 point);
bool Layout_SceneAuthoringState_DeletePathControlPoint(
    LineDrawingSceneAuthoringState* state,
    size_t path_index,
    size_t control_index);
bool Layout_SceneAuthoringState_SetLightPosition(LineDrawingSceneAuthoringState* state,
                                                 size_t light_index,
                                                 Vec3 point);
const char* Layout_SceneLightKind_Label(LineDrawingSceneLightKind kind);
const char* Layout_ScenePathRole_Name(LineDrawingScenePathRole role);
bool Layout_ScenePathRole_FromName(const char* name, LineDrawingScenePathRole* out_role);
const char* Layout_ScenePathTangentMode_Name(LineDrawingScenePathTangentMode mode);
bool Layout_ScenePathTangentMode_FromName(const char* name,
                                         LineDrawingScenePathTangentMode* out_mode);
const char* Layout_ScenePathPlaybackMode_Name(LineDrawingScenePathPlaybackMode mode);
bool Layout_ScenePathPlaybackMode_FromName(const char* name,
                                           LineDrawingScenePathPlaybackMode* out_mode);
