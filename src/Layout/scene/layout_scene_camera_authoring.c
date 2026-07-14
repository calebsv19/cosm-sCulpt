#include "Layout/scene/layout_scene_camera_authoring.h"
#include "Layout/scene/layout_scene_path_traversal.h"

#include "Layout/scene/layout_scene_path_geometry.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static void CameraCopy(char* dst, size_t size, const char* src) {
    if (dst && size > 0u) snprintf(dst, size, "%s", src ? src : "");
}

void Layout_SceneCamera_SetDefaults(LineDrawingSceneCamera* camera,
                                    const char* camera_id,
                                    const char* label,
                                    const char* path_id) {
    if (!camera) return;
    memset(camera, 0, sizeof(*camera));
    CameraCopy(camera->camera_id, sizeof(camera->camera_id), camera_id);
    CameraCopy(camera->label, sizeof(camera->label), label);
    CameraCopy(camera->path_id, sizeof(camera->path_id), path_id);
    camera->position = (Vec3){0.0f, -8.0f, 6.0f};
    camera->look_at_target = (Vec3){0.0f, 0.0f, 1.0f};
    camera->fixed_forward = Vec3_Normalize((Vec3){0.0f, 1.0f, -0.35f});
    camera->orientation_mode = LINE_DRAWING_SCENE_CAMERA_ORIENTATION_PATH_FACING;
    camera->vertical_fov_degrees = 50.0f;
    camera->near_clip = 0.1f;
    camera->far_clip = 250.0f;
}

const char* Layout_SceneCameraOrientationMode_Name(LineDrawingSceneCameraOrientationMode mode) {
    switch (mode) {
        case LINE_DRAWING_SCENE_CAMERA_ORIENTATION_LOOK_AT_TARGET: return "look_at_target";
        case LINE_DRAWING_SCENE_CAMERA_ORIENTATION_FIXED: return "fixed";
        case LINE_DRAWING_SCENE_CAMERA_ORIENTATION_PER_POINT: return "per_point";
        case LINE_DRAWING_SCENE_CAMERA_ORIENTATION_PATH_FACING:
        default: return "path_facing";
    }
}

bool Layout_SceneCameraOrientationMode_FromName(const char* name,
                                                LineDrawingSceneCameraOrientationMode* out_mode) {
    LineDrawingSceneCameraOrientationMode mode;
    if (!name) return false;
    if (strcmp(name, "look_at_target") == 0) mode = LINE_DRAWING_SCENE_CAMERA_ORIENTATION_LOOK_AT_TARGET;
    else if (strcmp(name, "fixed") == 0) mode = LINE_DRAWING_SCENE_CAMERA_ORIENTATION_FIXED;
    else if (strcmp(name, "per_point") == 0) mode = LINE_DRAWING_SCENE_CAMERA_ORIENTATION_PER_POINT;
    else if (strcmp(name, "path_facing") == 0) mode = LINE_DRAWING_SCENE_CAMERA_ORIENTATION_PATH_FACING;
    else return false;
    if (out_mode) *out_mode = mode;
    return true;
}

LineDrawingSceneCamera* Layout_SceneAuthoringState_FindCameraById(
    LineDrawingSceneAuthoringState* state, const char* camera_id) {
    if (!state || !camera_id || !camera_id[0]) return NULL;
    for (size_t i = 0u; i < state->camera_count; ++i) {
        if (strcmp(state->cameras[i].camera_id, camera_id) == 0) return &state->cameras[i];
    }
    return NULL;
}

const LineDrawingSceneCamera* Layout_SceneAuthoringState_FindCameraByIdConst(
    const LineDrawingSceneAuthoringState* state, const char* camera_id) {
    return Layout_SceneAuthoringState_FindCameraById((LineDrawingSceneAuthoringState*)state,
                                                     camera_id);
}

LineDrawingSceneCamera* Layout_SceneAuthoringState_FindCameraForPath(
    LineDrawingSceneAuthoringState* state, const LineDrawingScenePath* path) {
    LineDrawingSceneCamera* camera;
    if (!state || !path) return NULL;
    camera = Layout_SceneAuthoringState_FindCameraById(state, path->bound_camera_id);
    if (camera) return camera;
    for (size_t i = 0u; i < state->camera_count; ++i) {
        if (strcmp(state->cameras[i].path_id, path->path_id) == 0) return &state->cameras[i];
    }
    return NULL;
}

const LineDrawingSceneCamera* Layout_SceneAuthoringState_FindCameraForPathConst(
    const LineDrawingSceneAuthoringState* state, const LineDrawingScenePath* path) {
    return Layout_SceneAuthoringState_FindCameraForPath((LineDrawingSceneAuthoringState*)state,
                                                       path);
}

static LineDrawingSceneCamera* SelectedCamera(LineDrawingSceneAuthoringState* state) {
    if (!state || state->selected_kind != LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH ||
        state->selected_index >= state->path_count ||
        state->paths[state->selected_index].role != LINE_DRAWING_SCENE_PATH_ROLE_CAMERA) return NULL;
    return Layout_SceneAuthoringState_FindCameraForPath(state, &state->paths[state->selected_index]);
}

bool Layout_SceneAuthoringState_CycleSelectedCameraOrientation(LineDrawingSceneAuthoringState* state) {
    LineDrawingSceneCamera* camera = SelectedCamera(state);
    if (!camera) return false;
    camera->orientation_mode = (LineDrawingSceneCameraOrientationMode)
        (((int)camera->orientation_mode + 1) % 4);
    return true;
}

bool Layout_SceneAuthoringState_CycleSelectedCameraRoll(LineDrawingSceneAuthoringState* state) {
    LineDrawingSceneCamera* camera = SelectedCamera(state);
    if (!camera) return false;
    camera->roll_degrees += 15.0f;
    if (camera->roll_degrees > 30.0f) camera->roll_degrees = -30.0f;
    return true;
}

bool Layout_SceneAuthoringState_CycleSelectedCameraFov(LineDrawingSceneAuthoringState* state) {
    static const float values[] = {35.0f, 50.0f, 65.0f, 80.0f};
    LineDrawingSceneCamera* camera = SelectedCamera(state);
    size_t next = 0u;
    if (!camera) return false;
    for (size_t i = 0u; i < 4u; ++i) {
        if (fabsf(camera->vertical_fov_degrees - values[i]) < 0.01f) next = (i + 1u) % 4u;
    }
    camera->vertical_fov_degrees = values[next];
    return true;
}

bool Layout_SceneAuthoringState_CycleSelectedCameraClipPreset(LineDrawingSceneAuthoringState* state) {
    static const float near_values[] = {0.01f, 0.1f, 0.5f};
    static const float far_values[] = {100.0f, 250.0f, 1000.0f};
    LineDrawingSceneCamera* camera = SelectedCamera(state);
    size_t next = 0u;
    if (!camera) return false;
    for (size_t i = 0u; i < 3u; ++i) {
        if (fabsf(camera->near_clip - near_values[i]) < 0.0001f &&
            fabsf(camera->far_clip - far_values[i]) < 0.01f) next = (i + 1u) % 3u;
    }
    camera->near_clip = near_values[next];
    camera->far_clip = far_values[next];
    return true;
}

static Vec3 CameraPathForward(const LineDrawingScenePath* path, Vec3 position) {
    LineDrawingScenePathGeometry geometry = {0};
    if (path && Layout_ScenePathGeometry_Build(path, &geometry)) {
        for (size_t i = 1u; i < geometry.sample_count; ++i) {
            Vec3 forward = Vec3_Sub(geometry.samples[i].world, position);
            if (Vec3_Length(forward) > 0.0001f) return Vec3_Normalize(forward);
        }
    }
    return (Vec3){0.0f, 1.0f, 0.0f};
}

bool Layout_SceneCamera_EvaluatePose(const LineDrawingSceneCamera* camera,
                                     const LineDrawingScenePath* path,
                                     LineDrawingSceneCameraPose* out_pose) {
    LineDrawingSceneCameraPose pose = {0};
    Vec3 world_up = {0.0f, 0.0f, 1.0f};
    Vec3 right;
    if (!camera || !out_pose) return false;
    pose.position = path && path->control_point_count > 0u ? path->control_points[0]
                                                          : camera->position;
    if (camera->orientation_mode == LINE_DRAWING_SCENE_CAMERA_ORIENTATION_LOOK_AT_TARGET) {
        pose.forward = Vec3_Normalize(Vec3_Sub(camera->look_at_target, pose.position));
    } else if (camera->orientation_mode == LINE_DRAWING_SCENE_CAMERA_ORIENTATION_FIXED) {
        pose.forward = Vec3_Normalize(camera->fixed_forward);
    } else {
        pose.forward = CameraPathForward(path, pose.position);
    }
    if (Vec3_Length(pose.forward) < 0.0001f) pose.forward = (Vec3){0.0f, 1.0f, 0.0f};
    if (fabsf(Vec3_Dot(pose.forward, world_up)) > 0.98f) world_up = (Vec3){0.0f, 1.0f, 0.0f};
    right = Vec3_Normalize(Vec3_Cross(pose.forward, world_up));
    pose.up = Vec3_Normalize(Vec3_Cross(right, pose.forward));
    if (fabsf(camera->roll_degrees) > 0.001f) {
        const float radians = camera->roll_degrees * 0.01745329251994329577f;
        pose.up = Vec3_Normalize(Vec3_Add(Vec3_Scale(pose.up, cosf(radians)),
                                          Vec3_Scale(right, sinf(radians))));
    }
    *out_pose = pose;
    return true;
}

bool Layout_SceneCamera_EvaluatePoseAtNormalizedDistance(
    const LineDrawingSceneCamera* camera,
    const LineDrawingScenePath* path,
    float normalized_distance,
    LineDrawingSceneCameraPose* out_pose) {
    LineDrawingScenePathTraversalTable table = {0};
    LineDrawingScenePathTraversalSample sample = {0};
    LineDrawingSceneCamera sampled_camera;
    if (!camera || !path || !out_pose ||
        !Layout_ScenePathTraversal_Build(path, &table) ||
        !Layout_ScenePathTraversal_EvaluateNormalized(&table,
                                                      normalized_distance,
                                                      path->playback_mode,
                                                      &sample)) return false;
    sampled_camera = *camera;
    sampled_camera.position = sample.world;
    if (!Layout_SceneCamera_EvaluatePose(&sampled_camera, NULL, out_pose)) return false;
    out_pose->position = sample.world;
    if (camera->orientation_mode == LINE_DRAWING_SCENE_CAMERA_ORIENTATION_PATH_FACING ||
        camera->orientation_mode == LINE_DRAWING_SCENE_CAMERA_ORIENTATION_PER_POINT) {
        LineDrawingScenePathTraversalSample ahead = {0};
        const float epsilon = table.total_distance > 0.0f
            ? fmaxf(0.001f, 0.01f / table.total_distance) : 0.001f;
        if (Layout_ScenePathTraversal_EvaluateNormalized(&table,
                                                         normalized_distance + epsilon,
                                                         path->playback_mode,
                                                         &ahead) &&
            Vec3_Length(Vec3_Sub(ahead.world, sample.world)) > 0.0001f) {
            Vec3 world_up = {0.0f, 0.0f, 1.0f};
            Vec3 right;
            out_pose->forward = Vec3_Normalize(Vec3_Sub(ahead.world, sample.world));
            if (fabsf(Vec3_Dot(out_pose->forward, world_up)) > 0.98f)
                world_up = (Vec3){0.0f, 1.0f, 0.0f};
            right = Vec3_Normalize(Vec3_Cross(out_pose->forward, world_up));
            out_pose->up = Vec3_Normalize(Vec3_Cross(right, out_pose->forward));
        }
    }
    return true;
}

Vec3 Layout_SceneCamera_AimPoint(const LineDrawingSceneCamera* camera,
                                 const LineDrawingScenePath* path) {
    LineDrawingSceneCameraPose pose = {0};
    if (!camera) return (Vec3){0};
    if (camera->orientation_mode == LINE_DRAWING_SCENE_CAMERA_ORIENTATION_LOOK_AT_TARGET) {
        return camera->look_at_target;
    }
    if (Layout_SceneCamera_EvaluatePose(camera, path, &pose)) {
        return Vec3_Add(pose.position, Vec3_Scale(pose.forward, 4.0f));
    }
    return camera->position;
}

bool Layout_SceneCamera_SetAimPoint(LineDrawingSceneCamera* camera,
                                    Vec3 camera_position,
                                    Vec3 aim_point) {
    Vec3 forward;
    if (!camera) return false;
    if (camera->orientation_mode == LINE_DRAWING_SCENE_CAMERA_ORIENTATION_LOOK_AT_TARGET) {
        camera->look_at_target = aim_point;
        return true;
    }
    if (camera->orientation_mode != LINE_DRAWING_SCENE_CAMERA_ORIENTATION_FIXED) return false;
    forward = Vec3_Sub(aim_point, camera_position);
    if (Vec3_Length(forward) < 0.0001f) return false;
    camera->fixed_forward = Vec3_Normalize(forward);
    return true;
}
