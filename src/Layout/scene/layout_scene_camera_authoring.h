#pragma once

#include "Layout/scene/layout_scene_authoring.h"

#include <stdbool.h>

typedef struct {
    Vec3 position;
    Vec3 forward;
    Vec3 up;
} LineDrawingSceneCameraPose;

void Layout_SceneCamera_SetDefaults(LineDrawingSceneCamera* camera,
                                    const char* camera_id,
                                    const char* label,
                                    const char* path_id);
const char* Layout_SceneCameraOrientationMode_Name(LineDrawingSceneCameraOrientationMode mode);
bool Layout_SceneCameraOrientationMode_FromName(const char* name,
                                                LineDrawingSceneCameraOrientationMode* out_mode);
LineDrawingSceneCamera* Layout_SceneAuthoringState_FindCameraById(
    LineDrawingSceneAuthoringState* state, const char* camera_id);
const LineDrawingSceneCamera* Layout_SceneAuthoringState_FindCameraByIdConst(
    const LineDrawingSceneAuthoringState* state, const char* camera_id);
LineDrawingSceneCamera* Layout_SceneAuthoringState_FindCameraForPath(
    LineDrawingSceneAuthoringState* state, const LineDrawingScenePath* path);
const LineDrawingSceneCamera* Layout_SceneAuthoringState_FindCameraForPathConst(
    const LineDrawingSceneAuthoringState* state, const LineDrawingScenePath* path);
bool Layout_SceneAuthoringState_CycleSelectedCameraOrientation(LineDrawingSceneAuthoringState* state);
bool Layout_SceneAuthoringState_CycleSelectedCameraRoll(LineDrawingSceneAuthoringState* state);
bool Layout_SceneAuthoringState_CycleSelectedCameraFov(LineDrawingSceneAuthoringState* state);
bool Layout_SceneAuthoringState_CycleSelectedCameraClipPreset(LineDrawingSceneAuthoringState* state);
bool Layout_SceneCamera_EvaluatePose(const LineDrawingSceneCamera* camera,
                                     const LineDrawingScenePath* path,
                                     LineDrawingSceneCameraPose* out_pose);
bool Layout_SceneCamera_EvaluatePoseAtNormalizedDistance(
    const LineDrawingSceneCamera* camera,
    const LineDrawingScenePath* path,
    float normalized_distance,
    LineDrawingSceneCameraPose* out_pose);
bool Layout_SceneCamera_SetAimPoint(LineDrawingSceneCamera* camera,
                                    Vec3 camera_position,
                                    Vec3 aim_point);
Vec3 Layout_SceneCamera_AimPoint(const LineDrawingSceneCamera* camera,
                                 const LineDrawingScenePath* path);
