#pragma once
#include "Layout/scene/layout_scene_camera_authoring.h"
#include "cjson/cJSON.h"
#include "Layout/scene/layout_scene_path_traversal.h"
size_t CameraPoses_Control(const LineDrawingScenePath *path, size_t anchor);
float CameraPoses_Distance(const LineDrawingScenePath *path, size_t anchor);
float CameraPoses_Duration(const LineDrawingScenePath *path);
void CameraPoses_SetRotation(LineDrawingCameraKey *key, Vec3 forward, Vec3 up);
bool CameraPoses_Valid(const LineDrawingScenePath *path);
bool CameraPoses_Enable(LineDrawingScenePath *path, const LineDrawingSceneCamera *camera);
bool CameraPoses_Anchor(const LineDrawingScenePath *path, const LineDrawingSceneCamera *camera,
                        size_t anchor, LineDrawingSceneCameraPose *pose, float *fov);
bool CameraPoses_DistanceSample(const LineDrawingScenePath *path, float progress,
                               LineDrawingSceneCameraPose *pose, float *fov);
bool CameraPoses_Time(const LineDrawingScenePath *path, const LineDrawingSceneCamera *camera,
                      float seconds, LineDrawingSceneCameraPose *pose, float *fov);
bool CameraPoses_TimeCached(const LineDrawingScenePath *path, const LineDrawingSceneCamera *camera,
    float seconds, const LineDrawingScenePathTraversalTable *table,
    LineDrawingSceneCameraPose *pose, float *fov);
void CameraPoses_InsertKey(LineDrawingScenePath *path, size_t index);
void CameraPoses_RemoveKey(LineDrawingScenePath *path, size_t index);
bool CameraPoses_Add(LineDrawingScenePath *path, size_t after, Vec3 point,
                     const LineDrawingCameraKey *key);
bool CameraPoses_Move(LineDrawingScenePath *path, size_t from, size_t to);
bool CameraPoses_Remove(LineDrawingScenePath *path, size_t anchor);
cJSON *CameraPoses_ToJson(const LineDrawingScenePath *path);
/* Invalid optional records refuse atomically; legacy records remain readable. */
bool CameraPoses_FromJson(LineDrawingScenePath *path, const cJSON *node);
