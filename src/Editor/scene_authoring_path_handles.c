#include "Editor/scene_authoring_path_handles.h"

#include "Core/space_mode_adapter.h"
#include "Layout/Grid/grid.h"
#include "Layout/scene/layout_scene_path_geometry.h"
#include "Layout/scene/layout_scene_path_edit.h"
#include "Layout/scene/layout_scene_camera_authoring.h"
#include "Layout/scene/layout_scene_light_authoring.h"

#include <SDL2/SDL.h>
#include <math.h>
#include <string.h>

SceneAuthoringPathHandleDragState sceneAuthoringPathHandleDrag = {
    .active = false,
    .pick = {0},
    .mouseStartScreen = {0},
    .startWorld = {0},
    .projectedAxisVector = {0},
    .worldUnitsPerPixel = 0.0f,
    .historyCaptured = false
};

SceneAuthoringPathHandleRef SceneAuthoringPathHandleRef_None(void) {
    return (SceneAuthoringPathHandleRef){
        .kind = SCENE_AUTHORING_PATH_HANDLE_NONE,
        .light_index = 0u,
        .camera_index = 0u,
        .path_index = 0u,
        .control_index = 0u,
        .segment_index = 0u,
        .element_kind = LINE_DRAWING_SCENE_PATH_ELEMENT_NONE
    };
}

bool SceneAuthoringPathHandleRef_IsActive(SceneAuthoringPathHandleRef handle) {
    return handle.kind == SCENE_AUTHORING_PATH_HANDLE_CONTROL_POINT ||
           handle.kind == SCENE_AUTHORING_PATH_HANDLE_LIGHT_POSITION ||
           handle.kind == SCENE_AUTHORING_PATH_HANDLE_CAMERA_AIM ||
           handle.kind == SCENE_AUTHORING_PATH_HANDLE_LIGHT_AIM;
}

static LineDrawingScenePathElementRef SceneAuthoringPathHandles_ElementForHandle(
    const LineDrawingScenePath* path,
    SceneAuthoringPathHandleRef handle) {
    if (handle.element_kind == LINE_DRAWING_SCENE_PATH_ELEMENT_SEGMENT) {
        return Layout_ScenePathEdit_Segment(handle.segment_index);
    }
    return Layout_ScenePathEdit_ElementForControl(path, handle.control_index);
}

SceneAuthoringGizmoPickResult SceneAuthoringGizmoPickResult_None(void) {
    return (SceneAuthoringGizmoPickResult){
        .handle = { .kind = SCENE_AUTHORING_PATH_HANDLE_NONE },
        .part = SCENE_AUTHORING_GIZMO_PART_NONE,
        .axis = GIZMO_AXIS_DIR_POS_X
    };
}

bool SceneAuthoringGizmoPickResult_IsActive(SceneAuthoringGizmoPickResult pick) {
    return SceneAuthoringPathHandleRef_IsActive(pick.handle) &&
           (pick.part == SCENE_AUTHORING_GIZMO_PART_CENTER ||
            (pick.part == SCENE_AUTHORING_GIZMO_PART_AXIS &&
             (pick.axis == GIZMO_AXIS_DIR_POS_X ||
              pick.axis == GIZMO_AXIS_DIR_POS_Y ||
              pick.axis == GIZMO_AXIS_DIR_POS_Z)));
}

static bool SceneAuthoringPathHandles_SelectedPathIndex(const LineDrawingSceneAuthoringState* authoring,
                                                       size_t* out_path_index) {
    if (out_path_index) *out_path_index = 0u;
    if (!authoring) return false;
    if (authoring->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH) {
        if (authoring->selected_index >= authoring->path_count) return false;
        if (out_path_index) *out_path_index = authoring->selected_index;
        return true;
    }
    if (authoring->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT) {
        const LineDrawingSceneLight* light = NULL;
        if (authoring->selected_index >= authoring->light_count) return false;
        light = &authoring->lights[authoring->selected_index];
        if (light->path_id[0] == '\0') return false;
        for (size_t i = 0u; i < authoring->path_count; ++i) {
            if (strncmp(authoring->paths[i].path_id,
                        light->path_id,
                        sizeof(authoring->paths[i].path_id)) == 0) {
                if (out_path_index) *out_path_index = i;
                return true;
            }
        }
    }
    return false;
}

static bool SceneAuthoringPathHandles_SelectedLightIndex(const LineDrawingSceneAuthoringState* authoring,
                                                        size_t* out_light_index) {
    if (out_light_index) *out_light_index = 0u;
    if (!authoring ||
        authoring->selected_kind != LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT ||
        authoring->selected_index >= authoring->light_count) {
        return false;
    }
    if (out_light_index) *out_light_index = authoring->selected_index;
    return true;
}

bool SceneAuthoringPathHandles_ShouldShow(const GlobalState* state) {
    const LineDrawingSceneAuthoringState* authoring = NULL;
    if (!state || state->workspaceMode == LINE_DRAWING_WORKSPACE_MODE_OBJECT) return false;
    authoring = &state->layout.sceneAuthoring;
    if (authoring->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH) {
        return authoring->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH &&
               authoring->selected_index < authoring->path_count;
    }
    if (authoring->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT) {
        return authoring->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT &&
               authoring->selected_index < authoring->light_count;
    }
    return false;
}

static Vec2 SceneAuthoringPathHandles_WorldToScreen(const GlobalState* state,
                                                    Vec3 world,
                                                    const SpaceViewContext* viewCtx) {
    return WorldToScreen(SpaceAdapter_ProjectToView(world, viewCtx), &state->grid);
}

static float SceneAuthoringPathHandles_DistancePointToSegment(Vec2 p,
                                                              Vec2 a,
                                                              Vec2 b);

static bool SceneAuthoringPathHandles_PointForHandle(const GlobalState* state,
                                                     SceneAuthoringPathHandleRef handle,
                                                     Vec3* out_point) {
    const LineDrawingSceneAuthoringState* authoring = NULL;
    if (!state || !out_point) return false;
    authoring = &state->layout.sceneAuthoring;
    if (handle.kind == SCENE_AUTHORING_PATH_HANDLE_CONTROL_POINT) {
        const LineDrawingScenePath* path = NULL;
        if (handle.path_index >= authoring->path_count) return false;
        path = &authoring->paths[handle.path_index];
        if (handle.element_kind == LINE_DRAWING_SCENE_PATH_ELEMENT_SEGMENT) return false;
        if (handle.control_index >= path->control_point_count) return false;
        *out_point = path->control_points[handle.control_index];
        return true;
    }
    if (handle.kind == SCENE_AUTHORING_PATH_HANDLE_LIGHT_POSITION) {
        if (handle.light_index >= authoring->light_count) return false;
        if (authoring->lights[handle.light_index].position_mode !=
            LINE_DRAWING_SCENE_LIGHT_POSITION_INDEPENDENT) return false;
        *out_point = authoring->lights[handle.light_index].position;
        return true;
    }
    if (handle.kind == SCENE_AUTHORING_PATH_HANDLE_LIGHT_AIM) {
        const LineDrawingSceneLight* light = NULL;
        const LineDrawingScenePath* path = NULL;
        if (handle.light_index >= authoring->light_count) return false;
        light = &authoring->lights[handle.light_index];
        path = Layout_SceneAuthoringState_FindPathByIdConst(authoring, light->path_id);
        *out_point = Layout_SceneLight_AimPoint(light, path);
        return true;
    }
    if (handle.kind == SCENE_AUTHORING_PATH_HANDLE_CAMERA_AIM) {
        const LineDrawingSceneCamera* camera = NULL;
        const LineDrawingScenePath* path = NULL;
        if (handle.camera_index >= authoring->camera_count ||
            handle.path_index >= authoring->path_count) return false;
        camera = &authoring->cameras[handle.camera_index];
        path = &authoring->paths[handle.path_index];
        *out_point = Layout_SceneCamera_AimPoint(camera, path);
        return true;
    }
    return false;
}

static bool SceneAuthoringPathHandles_SelectedHandle(const GlobalState* state,
                                                     const EditorState* editor,
                                                     SceneAuthoringPathHandleRef* out_handle) {
    const LineDrawingSceneAuthoringState* authoring = NULL;
    SceneAuthoringPathHandleRef handle = SceneAuthoringPathHandleRef_None();
    if (out_handle) *out_handle = handle;
    if (!state || !editor) return false;
    authoring = &state->layout.sceneAuthoring;
    if (editor->selectedSceneAuthoringPathIndex >= 0 &&
        editor->selectedSceneAuthoringControlPointIndex >= 0) {
        size_t path_index = (size_t)editor->selectedSceneAuthoringPathIndex;
        size_t control_index = (size_t)editor->selectedSceneAuthoringControlPointIndex;
        if (path_index >= authoring->path_count) return false;
        if (control_index >= authoring->paths[path_index].control_point_count) return false;
        handle = (SceneAuthoringPathHandleRef){
            .kind = SCENE_AUTHORING_PATH_HANDLE_CONTROL_POINT,
            .light_index = 0u,
            .path_index = path_index,
            .control_index = control_index,
            .segment_index = editor->selectedSceneAuthoringPathSegmentIndex >= 0
                                 ? (size_t)editor->selectedSceneAuthoringPathSegmentIndex : 0u,
            .element_kind = (LineDrawingScenePathElementKind)
                editor->selectedSceneAuthoringPathElementKind
        };
        if (handle.element_kind == LINE_DRAWING_SCENE_PATH_ELEMENT_NONE) {
            handle.element_kind = Layout_ScenePathEdit_ElementForControl(
                &authoring->paths[path_index], control_index).kind;
        }
        if (out_handle) *out_handle = handle;
        return true;
    }
    if (editor->selectedSceneAuthoringLightPosition &&
        authoring->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT &&
        authoring->selected_index < authoring->light_count) {
        if (authoring->lights[authoring->selected_index].position_mode !=
            LINE_DRAWING_SCENE_LIGHT_POSITION_INDEPENDENT) return false;
        handle = (SceneAuthoringPathHandleRef){
            .kind = SCENE_AUTHORING_PATH_HANDLE_LIGHT_POSITION,
            .light_index = authoring->selected_index,
            .path_index = 0u,
            .control_index = 0u
        };
        if (out_handle) *out_handle = handle;
        return true;
    }
    if (editor->selectedSceneAuthoringLightAim &&
        authoring->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT &&
        authoring->selected_index < authoring->light_count) {
        const LineDrawingSceneLight* light = &authoring->lights[authoring->selected_index];
        if (light->kind != LINE_DRAWING_SCENE_LIGHT_POINT) {
            handle.kind = SCENE_AUTHORING_PATH_HANDLE_LIGHT_AIM;
            handle.light_index = authoring->selected_index;
            if (out_handle) *out_handle = handle;
            return true;
        }
    }
    if (editor->selectedSceneAuthoringCameraAim &&
        authoring->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH &&
        authoring->selected_index < authoring->path_count) {
        const LineDrawingScenePath* path = &authoring->paths[authoring->selected_index];
        const LineDrawingSceneCamera* camera =
            Layout_SceneAuthoringState_FindCameraForPathConst(authoring, path);
        if (camera &&
            (camera->orientation_mode == LINE_DRAWING_SCENE_CAMERA_ORIENTATION_LOOK_AT_TARGET ||
             camera->orientation_mode == LINE_DRAWING_SCENE_CAMERA_ORIENTATION_FIXED)) {
            handle.kind = SCENE_AUTHORING_PATH_HANDLE_CAMERA_AIM;
            handle.path_index = authoring->selected_index;
            handle.camera_index = (size_t)(camera - authoring->cameras);
            if (out_handle) *out_handle = handle;
            return true;
        }
    }
    return false;
}

enum {
    SCENE_AUTHORING_GIZMO_AXIS_LENGTH_PX = 58,
    SCENE_AUTHORING_GIZMO_ENDPOINT_HALF_EXTENT_PX = 5,
    SCENE_AUTHORING_GIZMO_PICK_RADIUS_PX = 10,
    SCENE_AUTHORING_GIZMO_CENTER_RADIUS_PX = 8,
    SCENE_AUTHORING_GIZMO_AXIS_CENTER_EXCLUSION_PX = 11
};

static Vec3 SceneAuthoringPathHandles_AxisVector(int axis_index) {
    switch (axis_index) {
        case 0: return (Vec3){ 1.0f, 0.0f, 0.0f };
        case 1: return (Vec3){ 0.0f, 1.0f, 0.0f };
        case 2: return (Vec3){ 0.0f, 0.0f, 1.0f };
        default: return (Vec3){ 0.0f, 0.0f, 0.0f };
    }
}

static SDL_Color SceneAuthoringPathHandles_AxisColor(int axis_index) {
    switch (axis_index) {
        case 0: return (SDL_Color){ 245, 95, 80, 235 };
        case 1: return (SDL_Color){ 95, 220, 125, 235 };
        case 2: return (SDL_Color){ 100, 150, 255, 235 };
        default: return (SDL_Color){ 230, 230, 230, 235 };
    }
}

static bool SceneAuthoringPathHandles_GizmoAxisScreenSegment(
    const GlobalState* state,
    const SpaceViewContext* viewCtx,
    Vec3 origin,
    int axis_index,
    Vec2* out_center,
    Vec2* out_endpoint) {
    Vec2 center = {0};
    Vec2 projected = {0};
    Vec2 direction = {0};
    float length = 0.0f;
    if (!state || !viewCtx || !out_center || !out_endpoint ||
        axis_index < 0 || axis_index > 2) {
        return false;
    }
    center = SceneAuthoringPathHandles_WorldToScreen(state, origin, viewCtx);
    projected = SceneAuthoringPathHandles_WorldToScreen(
        state,
        Vec3_Add(origin, SceneAuthoringPathHandles_AxisVector(axis_index)),
        viewCtx);
    direction = (Vec2){ projected.x - center.x, projected.y - center.y };
    length = sqrtf(direction.x * direction.x + direction.y * direction.y);
    if (!isfinite(length) || length <= 0.001f) return false;
    direction.x /= length;
    direction.y /= length;
    *out_center = center;
    *out_endpoint = (Vec2){
        center.x + direction.x * (float)SCENE_AUTHORING_GIZMO_AXIS_LENGTH_PX,
        center.y + direction.y * (float)SCENE_AUTHORING_GIZMO_AXIS_LENGTH_PX
    };
    return true;
}

static GizmoAxisDirection SceneAuthoringPathHandles_AxisDirection(int axis_index) {
    switch (axis_index) {
        case 1: return GIZMO_AXIS_DIR_POS_Y;
        case 2: return GIZMO_AXIS_DIR_POS_Z;
        case 0:
        default: return GIZMO_AXIS_DIR_POS_X;
    }
}

static bool SceneAuthoringPathHandles_PickSelectedGizmo(const GlobalState* state,
                                                        const EditorState* editor,
                                                        int mouse_x,
                                                        int mouse_y,
                                                        SceneAuthoringGizmoPickResult* out_pick) {
    SpaceViewContext viewCtx = {0};
    SceneAuthoringPathHandleRef handle = SceneAuthoringPathHandleRef_None();
    Vec3 origin = {0};
    Vec2 mouse = { (float)mouse_x, (float)mouse_y };
    Vec2 center = {0};
    float best_axis_distance = 999999.0f;
    float best_endpoint_distance = 999999.0f;
    int best_axis = -1;
    if (out_pick) *out_pick = SceneAuthoringGizmoPickResult_None();
    if (!state || !editor || !SceneAuthoringPathHandles_ShouldShow(state)) return false;
    if (!SceneAuthoringPathHandles_SelectedHandle(state, editor, &handle)) return false;
    if (!SceneAuthoringPathHandles_PointForHandle(state, handle, &origin)) return false;
    viewCtx = SpaceAdapter_BuildViewContext(state);
    center = SceneAuthoringPathHandles_WorldToScreen(state, origin, &viewCtx);
    if (Vec2_Distance(mouse, center) <= (float)SCENE_AUTHORING_GIZMO_CENTER_RADIUS_PX) {
        if (out_pick) {
            out_pick->handle = handle;
            out_pick->part = SCENE_AUTHORING_GIZMO_PART_CENTER;
        }
        return true;
    }
    for (int axis = 0; axis < 3; ++axis) {
        Vec2 endpoint = {0};
        float distance = 0.0f;
        if (!SceneAuthoringPathHandles_GizmoAxisScreenSegment(state,
                                                               &viewCtx,
                                                               origin,
                                                               axis,
                                                               &center,
                                                               &endpoint)) {
            continue;
        }
        Vec2 axis_start = center;
        const float axis_length = Vec2_Distance(center, endpoint);
        const float endpoint_distance = Vec2_Distance(mouse, endpoint);
        if (axis_length <= (float)SCENE_AUTHORING_GIZMO_AXIS_CENTER_EXCLUSION_PX) continue;
        axis_start.x += (endpoint.x - center.x) *
                        ((float)SCENE_AUTHORING_GIZMO_AXIS_CENTER_EXCLUSION_PX / axis_length);
        axis_start.y += (endpoint.y - center.y) *
                        ((float)SCENE_AUTHORING_GIZMO_AXIS_CENTER_EXCLUSION_PX / axis_length);
        distance = SceneAuthoringPathHandles_DistancePointToSegment(mouse, axis_start, endpoint);
        if (distance <= (float)SCENE_AUTHORING_GIZMO_PICK_RADIUS_PX &&
            (distance < best_axis_distance - 0.01f ||
             (fabsf(distance - best_axis_distance) <= 0.01f &&
              endpoint_distance < best_endpoint_distance - 0.01f))) {
            best_axis_distance = distance;
            best_endpoint_distance = endpoint_distance;
            best_axis = axis;
        }
    }
    if (best_axis_distance > (float)SCENE_AUTHORING_GIZMO_PICK_RADIUS_PX) return false;
    if (out_pick) {
        out_pick->handle = handle;
        out_pick->part = SCENE_AUTHORING_GIZMO_PART_AXIS;
        out_pick->axis = SceneAuthoringPathHandles_AxisDirection(best_axis);
    }
    return true;
}

void SceneAuthoringPathHandles_Select(EditorState* editor,
                                      SceneAuthoringPathHandleRef handle) {
    if (!editor) return;
    editor->selectedSceneAuthoringPathIndex = -1;
    editor->selectedSceneAuthoringControlPointIndex = -1;
    editor->selectedSceneAuthoringPathElementKind = LINE_DRAWING_SCENE_PATH_ELEMENT_NONE;
    editor->selectedSceneAuthoringPathSegmentIndex = -1;
    editor->selectedSceneAuthoringLightPosition = false;
    editor->selectedSceneAuthoringLightAim = false;
    editor->selectedSceneAuthoringCameraAim = false;
    if (handle.kind == SCENE_AUTHORING_PATH_HANDLE_CONTROL_POINT) {
        editor->selectedSceneAuthoringPathIndex = (int)handle.path_index;
        editor->selectedSceneAuthoringPathElementKind = (int)handle.element_kind;
        if (handle.element_kind == LINE_DRAWING_SCENE_PATH_ELEMENT_SEGMENT) {
            editor->selectedSceneAuthoringPathSegmentIndex = (int)handle.segment_index;
        } else {
            editor->selectedSceneAuthoringControlPointIndex = (int)handle.control_index;
        }
    } else if (handle.kind == SCENE_AUTHORING_PATH_HANDLE_LIGHT_POSITION) {
        editor->selectedSceneAuthoringLightPosition = true;
    } else if (handle.kind == SCENE_AUTHORING_PATH_HANDLE_CAMERA_AIM) {
        editor->selectedSceneAuthoringCameraAim = true;
    } else if (handle.kind == SCENE_AUTHORING_PATH_HANDLE_LIGHT_AIM) {
        editor->selectedSceneAuthoringLightAim = true;
    }
}

bool SceneAuthoringPathHandles_Pick(const GlobalState* state,
                                    int mouse_x,
                                    int mouse_y,
                                    SceneAuthoringGizmoPickResult* out_pick) {
    const LineDrawingSceneAuthoringState* authoring = NULL;
    SpaceViewContext viewCtx = {0};
    SceneAuthoringPathHandleRef best = SceneAuthoringPathHandleRef_None();
    float best_dist = 999999.0f;
    int best_priority = 99;
    size_t light_index = 0u;
    size_t path_index = 0u;
    const float pick_radius = 12.0f;

    if (out_pick) *out_pick = SceneAuthoringGizmoPickResult_None();
    if (!SceneAuthoringPathHandles_ShouldShow(state)) return false;
    authoring = &state->layout.sceneAuthoring;
    if (SceneAuthoringPathHandles_PickSelectedGizmo(state,
                                                    &state->editor,
                                                    mouse_x,
                                                    mouse_y,
                                                    out_pick)) {
        return true;
    }
    viewCtx = SpaceAdapter_BuildViewContext(state);

    if (SceneAuthoringPathHandles_SelectedLightIndex(authoring, &light_index)) {
        const LineDrawingSceneLight* light = &authoring->lights[light_index];
        if (light->kind != LINE_DRAWING_SCENE_LIGHT_POINT) {
            SceneAuthoringPathHandleRef aim_candidate = {
                .kind = SCENE_AUTHORING_PATH_HANDLE_LIGHT_AIM,
                .light_index = light_index
            };
            Vec3 world = {0};
            if (SceneAuthoringPathHandles_PointForHandle(state, aim_candidate, &world)) {
                Vec2 screen = SceneAuthoringPathHandles_WorldToScreen(state, world, &viewCtx);
                float dist = Vec2_Distance(screen, (Vec2){(float)mouse_x, (float)mouse_y});
                if (dist < best_dist && dist <= pick_radius) {
                    best = aim_candidate;
                    best_dist = dist;
                    best_priority = 0;
                }
            }
        }
        SceneAuthoringPathHandleRef candidate = {
            .kind = SCENE_AUTHORING_PATH_HANDLE_LIGHT_POSITION,
            .light_index = light_index,
            .path_index = 0u,
            .control_index = 0u
        };
        Vec3 world = {0};
        if (SceneAuthoringPathHandles_PointForHandle(state, candidate, &world)) {
            Vec2 screen = SceneAuthoringPathHandles_WorldToScreen(state, world, &viewCtx);
            float dist = Vec2_Distance(screen, (Vec2){ (float)mouse_x, (float)mouse_y });
            if (dist < best_dist && dist <= pick_radius) {
                best = candidate;
                best_dist = dist;
                best_priority = 1;
            }
        }
    }

    if (SceneAuthoringPathHandles_SelectedPathIndex(authoring, &path_index)) {
        const LineDrawingScenePath* path = &authoring->paths[path_index];
        const LineDrawingSceneCamera* camera =
            Layout_SceneAuthoringState_FindCameraForPathConst(authoring, path);
        if (camera &&
            (camera->orientation_mode == LINE_DRAWING_SCENE_CAMERA_ORIENTATION_LOOK_AT_TARGET ||
             camera->orientation_mode == LINE_DRAWING_SCENE_CAMERA_ORIENTATION_FIXED)) {
            SceneAuthoringPathHandleRef candidate = {
                .kind = SCENE_AUTHORING_PATH_HANDLE_CAMERA_AIM,
                .camera_index = (size_t)(camera - authoring->cameras),
                .path_index = path_index
            };
            Vec3 world = Layout_SceneCamera_AimPoint(camera, path);
            Vec2 screen = SceneAuthoringPathHandles_WorldToScreen(state, world, &viewCtx);
            float dist = Vec2_Distance(screen, (Vec2){(float)mouse_x, (float)mouse_y});
            if (dist <= pick_radius &&
                (0 < best_priority || (best_priority == 0 && dist < best_dist))) {
                best = candidate;
                best_dist = dist;
                best_priority = 0;
            }
        }
        for (size_t i = 0u; i < path->control_point_count; ++i) {
            const LineDrawingScenePathElementRef element =
                Layout_ScenePathEdit_ElementForControl(path, i);
            const int priority = element.kind == LINE_DRAWING_SCENE_PATH_ELEMENT_ANCHOR ? 0 : 1;
            SceneAuthoringPathHandleRef candidate = {
                .kind = SCENE_AUTHORING_PATH_HANDLE_CONTROL_POINT,
                .light_index = light_index,
                .path_index = path_index,
                .control_index = i,
                .segment_index = 0u,
                .element_kind = element.kind
            };
            Vec2 screen =
                SceneAuthoringPathHandles_WorldToScreen(state, path->control_points[i], &viewCtx);
            float dist = Vec2_Distance(screen, (Vec2){ (float)mouse_x, (float)mouse_y });
            if (dist <= pick_radius &&
                (priority < best_priority ||
                 (priority == best_priority && dist < best_dist))) {
                best = candidate;
                best_dist = dist;
                best_priority = priority;
            }
        }
        if (!SceneAuthoringPathHandleRef_IsActive(best)) {
            LineDrawingScenePathGeometry geometry = {0};
            float rail_dist = 999999.0f;
            size_t rail_segment = 0u;
            if (Layout_ScenePathGeometry_Build(path, &geometry)) {
                for (size_t i = 1u; i < geometry.sample_count; ++i) {
                    Vec2 a = SceneAuthoringPathHandles_WorldToScreen(
                        state, geometry.samples[i - 1u].world, &viewCtx);
                    Vec2 b = SceneAuthoringPathHandles_WorldToScreen(
                        state, geometry.samples[i].world, &viewCtx);
                    const float dist = SceneAuthoringPathHandles_DistancePointToSegment(
                        (Vec2){(float)mouse_x, (float)mouse_y}, a, b);
                    if (dist < rail_dist) {
                        rail_dist = dist;
                        rail_segment = geometry.samples[i].source_segment;
                    }
                }
            }
            if (rail_dist <= 8.0f) {
                best = (SceneAuthoringPathHandleRef){
                    .kind = SCENE_AUTHORING_PATH_HANDLE_CONTROL_POINT,
                    .path_index = path_index,
                    .segment_index = rail_segment,
                    .element_kind = LINE_DRAWING_SCENE_PATH_ELEMENT_SEGMENT
                };
                best_dist = rail_dist;
                best_priority = 2;
            }
        }
    }

    if (!SceneAuthoringPathHandleRef_IsActive(best)) return false;
    if (out_pick) {
        out_pick->handle = best;
        out_pick->part = SCENE_AUTHORING_GIZMO_PART_CENTER;
    }
    return true;
}

static float SceneAuthoringPathHandles_DistancePointToSegment(Vec2 p,
                                                              Vec2 a,
                                                              Vec2 b) {
    Vec2 ab = { b.x - a.x, b.y - a.y };
    Vec2 ap = { p.x - a.x, p.y - a.y };
    float len2 = ab.x * ab.x + ab.y * ab.y;
    float t = 0.0f;
    Vec2 closest = a;
    if (len2 > 0.0001f) {
        t = (ap.x * ab.x + ap.y * ab.y) / len2;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        closest.x = a.x + ab.x * t;
        closest.y = a.y + ab.y * t;
    }
    return Vec2_Distance(p, closest);
}

static float SceneAuthoringPathHandles_ClosestSegmentParameter(Vec2 p, Vec2 a, Vec2 b) {
    const Vec2 ab = { b.x - a.x, b.y - a.y };
    const Vec2 ap = { p.x - a.x, p.y - a.y };
    const float len2 = ab.x * ab.x + ab.y * ab.y;
    float t = len2 > 0.0001f ? (ap.x * ab.x + ap.y * ab.y) / len2 : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t;
}

static bool SceneAuthoringPathHandles_NearestGeometryForScreenPoint(
    const GlobalState* state,
    const LineDrawingScenePath* path,
    const SpaceViewContext* viewCtx,
    int mouse_x,
    int mouse_y,
    LineDrawingScenePathGeometry* out_geometry,
    size_t* out_sample_index,
    float* out_sample_t) {
    Vec2 mouse = { (float)mouse_x, (float)mouse_y };
    LineDrawingScenePathGeometry geometry = {0};
    size_t best_index = 0u;
    float best_t = 0.0f;
    float best_dist = 999999.0f;
    if (!state || !path || !viewCtx ||
        !Layout_ScenePathGeometry_Build(path, &geometry) || geometry.sample_count < 2u) {
        return false;
    }
    for (size_t i = 1u; i < geometry.sample_count; ++i) {
        Vec2 a = SceneAuthoringPathHandles_WorldToScreen(state,
                                                         geometry.samples[i - 1u].world,
                                                         viewCtx);
        Vec2 b = SceneAuthoringPathHandles_WorldToScreen(state,
                                                         geometry.samples[i].world,
                                                         viewCtx);
        float dist = SceneAuthoringPathHandles_DistancePointToSegment(mouse, a, b);
        if (dist < best_dist) {
            best_dist = dist;
            best_index = i;
            best_t = SceneAuthoringPathHandles_ClosestSegmentParameter(mouse, a, b);
        }
    }
    if (out_geometry) *out_geometry = geometry;
    if (out_sample_index) *out_sample_index = best_index;
    if (out_sample_t) *out_sample_t = best_t;
    return true;
}

bool SceneAuthoringPathHandles_InsertControlPointAtScreen(GlobalState* state,
                                                          EditorState* editor,
                                                          int mouse_x,
                                                          int mouse_y,
                                                          SceneAuthoringPathHandleRef* out_handle) {
    LineDrawingSceneAuthoringState* authoring = NULL;
    SpaceViewContext viewCtx = {0};
    Vec3 world = {0};
    size_t path_index = 0u;
    size_t insert_index = 0u;
    size_t sample_index = 0u;
    float sample_t = 0.0f;
    LineDrawingScenePathGeometry geometry = {0};
    SceneAuthoringPathHandleRef inserted = SceneAuthoringPathHandleRef_None();
    if (out_handle) *out_handle = inserted;
    if (!state || !editor || !SceneAuthoringPathHandles_ShouldShow(state)) return false;
    authoring = &state->layout.sceneAuthoring;
    if (!SceneAuthoringPathHandles_SelectedPathIndex(authoring, &path_index)) return false;
    if (path_index >= authoring->path_count) return false;
    viewCtx = SpaceAdapter_BuildViewContext(state);
    if (!SpaceAdapter_ScreenToWorld(mouse_x,
                                    mouse_y,
                                    &state->grid,
                                    &viewCtx,
                                    true,
                                    &world)) {
        return false;
    }
    if (!SceneAuthoringPathHandles_NearestGeometryForScreenPoint(
        state,
        &authoring->paths[path_index],
        &viewCtx,
        mouse_x,
        mouse_y,
        &geometry,
        &sample_index,
        &sample_t)) {
        return false;
    }
    if (geometry.kind == LINE_DRAWING_SCENE_PATH_GEOMETRY_CUBIC_BEZIER) {
        const LineDrawingScenePathSample* a = &geometry.samples[sample_index - 1u];
        const LineDrawingScenePathSample* b = &geometry.samples[sample_index];
        const float a_t = a->source_segment == b->source_segment ? a->segment_t : 0.0f;
        const float curve_t = a_t + (b->segment_t - a_t) * sample_t;
        if (authoring->paths[path_index].control_point_count + 3u >
            LINE_DRAWING_SCENE_AUTHORING_MAX_PATH_POINTS) {
            return false;
        }
        LineDrawingScenePath edited_path = authoring->paths[path_index];
        LineDrawingScenePathElementRef inserted_element = {0};
        if (!Layout_ScenePathEdit_SplitSegment(&edited_path,
                                               b->source_segment,
                                               curve_t,
                                               &inserted_element)) {
            return false;
        }
        Editor_HistoryCapture(editor, &state->layout);
        authoring->paths[path_index] = edited_path;
        insert_index = inserted_element.control_index;
    } else {
        if (authoring->paths[path_index].control_point_count >=
            LINE_DRAWING_SCENE_AUTHORING_MAX_PATH_POINTS) {
            return false;
        }
        insert_index = geometry.samples[sample_index].source_segment + 1u;
        LineDrawingScenePath edited_path = authoring->paths[path_index];
        if (edited_path.control_point_count >= LINE_DRAWING_SCENE_AUTHORING_MAX_PATH_POINTS) {
            return false;
        }
        for (size_t i = edited_path.control_point_count; i > insert_index; --i) {
            edited_path.control_points[i] = edited_path.control_points[i - 1u];
        }
        edited_path.control_points[insert_index] = world;
        ++edited_path.control_point_count;
        Editor_HistoryCapture(editor, &state->layout);
        authoring->paths[path_index] = edited_path;
    }
    inserted = (SceneAuthoringPathHandleRef){
        .kind = SCENE_AUTHORING_PATH_HANDLE_CONTROL_POINT,
        .light_index = 0u,
        .path_index = path_index,
        .control_index = insert_index,
        .segment_index = 0u,
        .element_kind = Layout_ScenePathEdit_ElementForControl(
            &authoring->paths[path_index], insert_index).kind
    };
    SceneAuthoringPathHandles_Select(editor, inserted);
    Global_FlagLayoutChanged();
    if (out_handle) *out_handle = inserted;
    return true;
}

bool SceneAuthoringPathHandles_DeleteSelectedControlPoint(GlobalState* state,
                                                          EditorState* editor) {
    LineDrawingSceneAuthoringState* authoring = NULL;
    size_t path_index = 0u;
    size_t control_index = 0u;
    if (!state || !editor) return false;
    if (!SceneAuthoringPathHandles_ShouldShow(state)) return false;
    if (editor->selectedSceneAuthoringPathIndex < 0 ||
        editor->selectedSceneAuthoringPathElementKind ==
            LINE_DRAWING_SCENE_PATH_ELEMENT_NONE) {
        return false;
    }
    path_index = (size_t)editor->selectedSceneAuthoringPathIndex;
    control_index = editor->selectedSceneAuthoringControlPointIndex >= 0
                        ? (size_t)editor->selectedSceneAuthoringControlPointIndex : 0u;
    authoring = &state->layout.sceneAuthoring;
    if (path_index >= authoring->path_count ||
        authoring->paths[path_index].control_point_count <= 2u) {
        return false;
    }
    LineDrawingScenePathElementRef element = {0};
    LineDrawingScenePathElementRef next_selection = {0};
    if (editor->selectedSceneAuthoringPathElementKind ==
        LINE_DRAWING_SCENE_PATH_ELEMENT_SEGMENT) {
        element = Layout_ScenePathEdit_Segment(
            (size_t)editor->selectedSceneAuthoringPathSegmentIndex);
    } else {
        if (control_index >= authoring->paths[path_index].control_point_count) return false;
        element = Layout_ScenePathEdit_ElementForControl(
            &authoring->paths[path_index], control_index);
    }
    LineDrawingScenePath edited_path = authoring->paths[path_index];
    if (!Layout_ScenePathEdit_DeleteElement(&edited_path, element, &next_selection)) {
        return false;
    }
    Editor_HistoryCapture(editor, &state->layout);
    authoring->paths[path_index] = edited_path;
    SceneAuthoringPathHandles_Select(editor, (SceneAuthoringPathHandleRef){
        .kind = SCENE_AUTHORING_PATH_HANDLE_CONTROL_POINT,
        .path_index = path_index,
        .control_index = next_selection.control_index,
        .segment_index = next_selection.segment_index,
        .element_kind = next_selection.kind
    });
    Global_FlagLayoutChanged();
    return true;
}

bool SceneAuthoringPathHandles_SetWorldPoint(GlobalState* state,
                                             SceneAuthoringPathHandleRef handle,
                                             Vec3 point) {
    if (!state) return false;
    if (handle.kind == SCENE_AUTHORING_PATH_HANDLE_CONTROL_POINT) {
        LineDrawingSceneAuthoringState* authoring = &state->layout.sceneAuthoring;
        LineDrawingScenePathElementRef element;
        bool changed;
        if (handle.path_index >= authoring->path_count) return false;
        element = SceneAuthoringPathHandles_ElementForHandle(
            &authoring->paths[handle.path_index], handle);
        changed = Layout_ScenePathEdit_SetElementWorldPoint(
            &authoring->paths[handle.path_index],
            element,
            point);
        if (changed && element.kind == LINE_DRAWING_SCENE_PATH_ELEMENT_ANCHOR &&
            element.control_index == 0u &&
            authoring->paths[handle.path_index].role == LINE_DRAWING_SCENE_PATH_ROLE_CAMERA) {
            LineDrawingSceneCamera* camera = Layout_SceneAuthoringState_FindCameraForPath(
                authoring, &authoring->paths[handle.path_index]);
            if (camera) camera->position = point;
        }
        return changed;
    }
    if (handle.kind == SCENE_AUTHORING_PATH_HANDLE_LIGHT_POSITION) {
        LineDrawingSceneAuthoringState* authoring = &state->layout.sceneAuthoring;
        if (handle.light_index >= authoring->light_count ||
            authoring->lights[handle.light_index].position_mode !=
                LINE_DRAWING_SCENE_LIGHT_POSITION_INDEPENDENT) return false;
        return Layout_SceneAuthoringState_SetLightPosition(&state->layout.sceneAuthoring,
                                                           handle.light_index,
                                                           point);
    }
    if (handle.kind == SCENE_AUTHORING_PATH_HANDLE_LIGHT_AIM) {
        LineDrawingSceneAuthoringState* authoring = &state->layout.sceneAuthoring;
        LineDrawingScenePath* path = NULL;
        if (handle.light_index >= authoring->light_count) return false;
        path = Layout_SceneAuthoringState_FindPathById(
            authoring, authoring->lights[handle.light_index].path_id);
        return Layout_SceneLight_SetAimPoint(&authoring->lights[handle.light_index], path, point);
    }
    if (handle.kind == SCENE_AUTHORING_PATH_HANDLE_CAMERA_AIM) {
        LineDrawingSceneAuthoringState* authoring = &state->layout.sceneAuthoring;
        LineDrawingSceneCameraPose pose = {0};
        if (handle.camera_index >= authoring->camera_count ||
            handle.path_index >= authoring->path_count ||
            !Layout_SceneCamera_EvaluatePoseAtNormalizedDistance(
                &authoring->cameras[handle.camera_index],
                &authoring->paths[handle.path_index],
                authoring->paths[handle.path_index].normalized_distance,
                &pose)) return false;
        return Layout_SceneCamera_SetAimPoint(&authoring->cameras[handle.camera_index],
                                              pose.position,
                                              point);
    }
    return false;
}

bool SceneAuthoringPathHandles_CycleSelectedTangentMode(GlobalState* state,
                                                        EditorState* editor) {
    LineDrawingSceneAuthoringState* authoring = NULL;
    LineDrawingScenePathElementRef element = {0};
    size_t path_index = 0u;
    size_t control_index = 0u;
    if (!state || !editor || editor->selectedSceneAuthoringPathIndex < 0 ||
        editor->selectedSceneAuthoringControlPointIndex < 0) return false;
    authoring = &state->layout.sceneAuthoring;
    path_index = (size_t)editor->selectedSceneAuthoringPathIndex;
    control_index = (size_t)editor->selectedSceneAuthoringControlPointIndex;
    if (path_index >= authoring->path_count ||
        control_index >= authoring->paths[path_index].control_point_count) return false;
    element = Layout_ScenePathEdit_ElementForControl(
        &authoring->paths[path_index], control_index);
    if (element.kind == LINE_DRAWING_SCENE_PATH_ELEMENT_NONE) return false;
    LineDrawingScenePath edited_path = authoring->paths[path_index];
    if (!Layout_ScenePathEdit_CycleAnchorMode(&edited_path, element.anchor_index)) return false;
    Editor_HistoryCapture(editor, &state->layout);
    authoring->paths[path_index] = edited_path;
    Global_FlagLayoutChanged();
    return true;
}

bool BeginSceneAuthoringPathHandleDragSession(GlobalState* state,
                                              EditorState* editor,
                                              SceneAuthoringGizmoPickResult pick,
                                              int mouse_x,
                                              int mouse_y) {
    Vec3 start_world = {0};
    if (!state || !editor || !SceneAuthoringGizmoPickResult_IsActive(pick)) return false;
    if (!SceneAuthoringPathHandles_ShouldShow(state)) return false;
    SceneAuthoringPathHandles_Select(editor, pick.handle);
    if (!SceneAuthoringPathHandles_PointForHandle(state, pick.handle, &start_world)) return false;
    sceneAuthoringPathHandleDrag.active = true;
    sceneAuthoringPathHandleDrag.pick = pick;
    sceneAuthoringPathHandleDrag.mouseStartScreen = (Vec2){(float)mouse_x, (float)mouse_y};
    sceneAuthoringPathHandleDrag.startWorld = start_world;
    sceneAuthoringPathHandleDrag.projectedAxisVector = (Vec2){0};
    sceneAuthoringPathHandleDrag.worldUnitsPerPixel = 0.0f;
    if (pick.part == SCENE_AUTHORING_GIZMO_PART_AXIS) {
        SpaceViewContext view_ctx = SpaceAdapter_BuildViewContext(state);
        Vec2 center = {0};
        Vec2 endpoint = {0};
        int axis_index = pick.axis == GIZMO_AXIS_DIR_POS_Y ? 1 :
                         pick.axis == GIZMO_AXIS_DIR_POS_Z ? 2 : 0;
        if (!SceneAuthoringPathHandles_GizmoAxisScreenSegment(state, &view_ctx, start_world,
                                                               axis_index, &center, &endpoint)) {
            return false;
        }
        sceneAuthoringPathHandleDrag.projectedAxisVector =
            (Vec2){endpoint.x - center.x, endpoint.y - center.y};
        sceneAuthoringPathHandleDrag.worldUnitsPerPixel =
            1.0f / Vec2_Distance(
                SceneAuthoringPathHandles_WorldToScreen(state, start_world, &view_ctx),
                SceneAuthoringPathHandles_WorldToScreen(
                    state, Vec3_Add(start_world, GizmoAxisDirection_WorldVector(pick.axis)),
                    &view_ctx));
    }
    sceneAuthoringPathHandleDrag.historyCaptured = false;
    editor->activeSceneAuthoringGizmoPart = (int)pick.part;
    editor->activeSceneAuthoringGizmoAxis = pick.part == SCENE_AUTHORING_GIZMO_PART_AXIS
                                               ? (int)pick.axis : -1;
    editor->isPreciseDrag = (SDL_GetModState() & KMOD_ALT) != 0;
    return true;
}

void ResetSceneAuthoringPathHandleDrag(EditorState* editor) {
    sceneAuthoringPathHandleDrag.active = false;
    sceneAuthoringPathHandleDrag.pick = SceneAuthoringGizmoPickResult_None();
    sceneAuthoringPathHandleDrag.historyCaptured = false;
    if (editor) {
        editor->isPreciseDrag = false;
        editor->activeSceneAuthoringGizmoPart = SCENE_AUTHORING_GIZMO_PART_NONE;
        editor->activeSceneAuthoringGizmoAxis = -1;
    }
}

void UpdateSceneAuthoringPathHandleDragPosition(int mouse_x, int mouse_y) {
    GlobalState* state = Global_Get();
    SpaceViewContext viewCtx = {0};
    Vec3 world = {0};
    bool precise = false;
    if (!state || !sceneAuthoringPathHandleDrag.active) return;
    if (!SceneAuthoringGizmoPickResult_IsActive(sceneAuthoringPathHandleDrag.pick)) return;
    precise = (SDL_GetModState() & KMOD_ALT) != 0;
    state->editor.isPreciseDrag = precise;
    if (sceneAuthoringPathHandleDrag.pick.part == SCENE_AUTHORING_GIZMO_PART_AXIS) {
        float signed_pixels = GizmoDrag_SignedPixelsAlongAxis(
            sceneAuthoringPathHandleDrag.mouseStartScreen,
            (Vec2){(float)mouse_x, (float)mouse_y},
            sceneAuthoringPathHandleDrag.projectedAxisVector);
        float distance = GizmoDrag_DistanceWorldFromPixels(
            signed_pixels, sceneAuthoringPathHandleDrag.worldUnitsPerPixel);
        world = GizmoDrag_ApplyAxisDistance(sceneAuthoringPathHandleDrag.startWorld,
                                            sceneAuthoringPathHandleDrag.pick.axis,
                                            distance);
    } else {
        viewCtx = SpaceAdapter_BuildViewContext(state);
        if (!SpaceAdapter_ScreenToWorld(mouse_x,
                                    mouse_y,
                                    &state->grid,
                                    &viewCtx,
                                    !precise,
                                    &world)) {
            return;
        }
    }
    if (!sceneAuthoringPathHandleDrag.historyCaptured) {
        Editor_HistoryCapture(&state->editor, &state->layout);
        sceneAuthoringPathHandleDrag.historyCaptured = true;
    }
    if (SceneAuthoringPathHandles_SetWorldPoint(state,
                                                sceneAuthoringPathHandleDrag.pick.handle,
                                                world)) {
        Global_FlagLayoutChanged();
    }
}

static void SceneAuthoringPathHandles_DrawSquareMarker(SDL_Renderer* renderer,
                                                       int cx,
                                                       int cy,
                                                       int half_extent,
                                                       bool selected) {
    SDL_Rect rect = { cx - half_extent, cy - half_extent, half_extent * 2 + 1, half_extent * 2 + 1 };
    if (!renderer || half_extent <= 0) return;
    if (selected) {
        (void)SDL_RenderDrawRect(renderer, &rect);
        (void)SDL_RenderDrawLine(renderer, cx - half_extent - 2, cy, cx + half_extent + 2, cy);
        (void)SDL_RenderDrawLine(renderer, cx, cy - half_extent - 2, cx, cy + half_extent + 2);
    } else {
        (void)SDL_RenderFillRect(renderer, &rect);
    }
}

static void SceneAuthoringPathHandles_DrawLineWithHalo(SDL_Renderer* renderer,
                                                       Vec2 a,
                                                       Vec2 b,
                                                       SDL_Color halo,
                                                       SDL_Color line) {
    if (!renderer) return;
    SDL_SetRenderDrawColor(renderer, halo.r, halo.g, halo.b, halo.a);
    for (int ox = -1; ox <= 1; ++ox) {
        for (int oy = -1; oy <= 1; ++oy) {
            if (ox == 0 && oy == 0) continue;
            (void)SDL_RenderDrawLine(renderer,
                                     (int)a.x + ox,
                                     (int)a.y + oy,
                                     (int)b.x + ox,
                                     (int)b.y + oy);
        }
    }
    SDL_SetRenderDrawColor(renderer, line.r, line.g, line.b, line.a);
    (void)SDL_RenderDrawLine(renderer, (int)a.x, (int)a.y, (int)b.x, (int)b.y);
}

static void SceneAuthoringPathHandles_DrawPath(SDL_Renderer* renderer,
                                               const GlobalState* state,
                                               const SpaceViewContext* viewCtx,
                                               const LineDrawingScenePath* path,
                                               int selected_segment) {
    LineDrawingScenePathGeometry geometry = {0};
    if (!renderer || !state || !viewCtx || !path ||
        !Layout_ScenePathGeometry_Build(path, &geometry) || geometry.sample_count < 2u) return;
    for (size_t i = 1u; i < geometry.sample_count; ++i) {
        Vec2 a = SceneAuthoringPathHandles_WorldToScreen(state,
                                                         geometry.samples[i - 1u].world,
                                                         viewCtx);
        Vec2 b = SceneAuthoringPathHandles_WorldToScreen(state,
                                                         geometry.samples[i].world,
                                                         viewCtx);
        const bool selected = selected_segment >= 0 &&
                              geometry.samples[i].source_segment ==
                                  (size_t)selected_segment;
        SceneAuthoringPathHandles_DrawLineWithHalo(renderer,
                                                   a,
                                                   b,
                                                   (SDL_Color){ 38, 26, 8, 220 },
                                                   selected
                                                       ? (SDL_Color){ 255, 245, 165, 255 }
                                                       : (SDL_Color){ 255, 218, 80, 245 });
    }
}

static void SceneAuthoringPathHandles_DrawSelectedGizmo(SDL_Renderer* renderer,
                                                        const GlobalState* state,
                                                        const SpaceViewContext* viewCtx,
                                                        Vec3 origin,
                                                        int hovered_axis,
                                                        int active_axis) {
    Vec2 center = {0};
    if (!renderer || !state || !viewCtx) return;
    center = SceneAuthoringPathHandles_WorldToScreen(state, origin, viewCtx);
    for (int axis = 0; axis < 3; ++axis) {
        SDL_Color color = SceneAuthoringPathHandles_AxisColor(axis);
        Vec2 endpoint = {0};
        const bool hovered = axis == hovered_axis;
        const bool active = axis == active_axis;
        if (!SceneAuthoringPathHandles_GizmoAxisScreenSegment(state,
                                                               viewCtx,
                                                               origin,
                                                               axis,
                                                               &center,
                                                               &endpoint)) {
            continue;
        }
        if (hovered || active) {
            color.r = (Uint8)fminf(255.0f, (float)color.r + 45.0f);
            color.g = (Uint8)fminf(255.0f, (float)color.g + 45.0f);
            color.b = (Uint8)fminf(255.0f, (float)color.b + 45.0f);
            SceneAuthoringPathHandles_DrawLineWithHalo(renderer,
                                                       (Vec2){center.x - 1.0f, center.y},
                                                       (Vec2){endpoint.x - 1.0f, endpoint.y},
                                                       (SDL_Color){ 10, 10, 10, 235 },
                                                       color);
        }
        SceneAuthoringPathHandles_DrawLineWithHalo(renderer,
                                                   center,
                                                   endpoint,
                                                   (SDL_Color){ 10, 10, 10, 220 },
                                                   color);
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SceneAuthoringPathHandles_DrawSquareMarker(renderer,
                                                   (int)endpoint.x,
                                                   (int)endpoint.y,
                                                   (hovered || active)
                                                       ? SCENE_AUTHORING_GIZMO_ENDPOINT_HALF_EXTENT_PX + 2
                                                       : SCENE_AUTHORING_GIZMO_ENDPOINT_HALF_EXTENT_PX,
                                                   false);
    }
}

static void SceneAuthoringPathHandles_DrawCameraLine(SDL_Renderer* renderer,
                                                     const GlobalState* state,
                                                     const SpaceViewContext* view_ctx,
                                                     Vec3 a,
                                                     Vec3 b,
                                                     SDL_Color color) {
    SceneAuthoringPathHandles_DrawLineWithHalo(
        renderer,
        SceneAuthoringPathHandles_WorldToScreen(state, a, view_ctx),
        SceneAuthoringPathHandles_WorldToScreen(state, b, view_ctx),
        (SDL_Color){12, 18, 26, 220}, color);
}

static void SceneAuthoringPathHandles_DrawCamera(SDL_Renderer* renderer,
                                                 const GlobalState* state,
                                                 const SpaceViewContext* view_ctx,
                                                 const LineDrawingScenePath* path,
                                                 const LineDrawingSceneCamera* camera,
                                                 bool aim_selected,
                                                 bool aim_hovered,
                                                 int hovered_axis,
                                                 int active_axis) {
    LineDrawingSceneCameraPose pose = {0};
    LineDrawingScenePathGeometry geometry = {0};
    Vec3 right;
    Vec3 body_right;
    Vec3 body_up;
    Vec3 near_center;
    Vec3 far_center;
    Vec3 near_corners[4];
    Vec3 far_corners[4];
    const float aspect = 16.0f / 9.0f;
    float half_angle;
    float near_height;
    float far_height;
    const SDL_Color camera_color = {110, 225, 255, 245};
    const SDL_Color frustum_color = {95, 175, 230, 220};
    if (!renderer || !state || !view_ctx || !path || !camera ||
        !Layout_SceneCamera_EvaluatePoseAtNormalizedDistance(
            camera, path, path->normalized_distance, &pose)) return;
    right = Vec3_Normalize(Vec3_Cross(pose.forward, pose.up));
    body_right = Vec3_Scale(right, 0.42f);
    body_up = Vec3_Scale(pose.up, 0.28f);
    SceneAuthoringPathHandles_DrawCameraLine(renderer, state, view_ctx,
        Vec3_Add(pose.position, Vec3_Add(body_right, body_up)),
        Vec3_Add(pose.position, Vec3_Sub(body_right, body_up)), camera_color);
    SceneAuthoringPathHandles_DrawCameraLine(renderer, state, view_ctx,
        Vec3_Sub(pose.position, Vec3_Add(body_right, body_up)),
        Vec3_Add(pose.position, Vec3_Sub(body_right, body_up)), camera_color);
    SceneAuthoringPathHandles_DrawCameraLine(renderer, state, view_ctx,
        Vec3_Sub(pose.position, Vec3_Add(body_right, body_up)),
        Vec3_Add(pose.position, Vec3_Sub(body_up, body_right)), camera_color);
    SceneAuthoringPathHandles_DrawCameraLine(renderer, state, view_ctx,
        Vec3_Add(pose.position, Vec3_Add(body_right, body_up)),
        Vec3_Add(pose.position, Vec3_Sub(body_up, body_right)), camera_color);

    half_angle = camera->vertical_fov_degrees * 0.00872664625997164788f;
    near_center = Vec3_Add(pose.position, Vec3_Scale(pose.forward, 1.0f));
    far_center = Vec3_Add(pose.position, Vec3_Scale(pose.forward, 4.0f));
    near_height = tanf(half_angle);
    far_height = near_height * 4.0f;
    for (int i = 0; i < 4; ++i) {
        const float sx = (i == 0 || i == 3) ? -1.0f : 1.0f;
        const float sy = i < 2 ? -1.0f : 1.0f;
        near_corners[i] = Vec3_Add(near_center,
            Vec3_Add(Vec3_Scale(right, sx * near_height * aspect),
                     Vec3_Scale(pose.up, sy * near_height)));
        far_corners[i] = Vec3_Add(far_center,
            Vec3_Add(Vec3_Scale(right, sx * far_height * aspect),
                     Vec3_Scale(pose.up, sy * far_height)));
        SceneAuthoringPathHandles_DrawCameraLine(renderer, state, view_ctx,
                                                  pose.position, far_corners[i],
                                                  frustum_color);
        SceneAuthoringPathHandles_DrawCameraLine(renderer, state, view_ctx,
                                                  near_corners[i],
                                                  near_corners[(i + 1) % 4],
                                                  frustum_color);
        SceneAuthoringPathHandles_DrawCameraLine(renderer, state, view_ctx,
                                                  far_corners[i],
                                                  far_corners[(i + 1) % 4],
                                                  frustum_color);
    }

    if (Layout_ScenePathGeometry_Build(path, &geometry)) {
        for (size_t i = 0u; i < geometry.sample_count; i += 6u) {
            Vec2 marker = SceneAuthoringPathHandles_WorldToScreen(
                state, geometry.samples[i].world, view_ctx);
            SDL_SetRenderDrawColor(renderer, 100, 210, 245, 210);
            (void)SDL_RenderDrawLine(renderer, (int)marker.x - 2, (int)marker.y,
                                     (int)marker.x + 2, (int)marker.y);
            (void)SDL_RenderDrawLine(renderer, (int)marker.x, (int)marker.y - 2,
                                     (int)marker.x, (int)marker.y + 2);
        }
    }

    if (camera->orientation_mode == LINE_DRAWING_SCENE_CAMERA_ORIENTATION_LOOK_AT_TARGET ||
        camera->orientation_mode == LINE_DRAWING_SCENE_CAMERA_ORIENTATION_FIXED) {
        Vec3 aim = Layout_SceneCamera_AimPoint(camera, path);
        Vec2 screen = SceneAuthoringPathHandles_WorldToScreen(state, aim, view_ctx);
        SceneAuthoringPathHandles_DrawCameraLine(renderer, state, view_ctx,
                                                  pose.position, aim,
                                                  aim_selected || aim_hovered
                                                      ? (SDL_Color){255, 220, 110, 250}
                                                      : (SDL_Color){190, 185, 105, 220});
        SDL_SetRenderDrawColor(renderer, 255, 220, 110, 245);
        SceneAuthoringPathHandles_DrawSquareMarker(renderer, (int)screen.x, (int)screen.y,
                                                   aim_selected ? 5 : aim_hovered ? 4 : 3,
                                                   aim_selected || aim_hovered);
        if (aim_selected) {
            SceneAuthoringPathHandles_DrawSelectedGizmo(renderer, state, view_ctx, aim,
                                                        hovered_axis, active_axis);
        }
    }
}

/* Draws renderer-neutral light body, aim, spot cone, or area-size authoring previews. */
static void SceneAuthoringPathHandles_DrawLight(SDL_Renderer* renderer,
                                                const GlobalState* state,
                                                const SpaceViewContext* view_ctx,
                                                const LineDrawingScenePath* path,
                                                const LineDrawingSceneLight* light,
                                                bool position_selected,
                                                bool aim_selected,
                                                bool aim_hovered,
                                                int hovered_axis,
                                                int active_axis) {
    Vec3 position;
    Vec3 direction;
    Vec3 aim;
    Vec3 reference;
    Vec3 right;
    Vec3 up;
    Vec2 screen;
    SDL_Color color;
    if (!renderer || !state || !view_ctx || !light) return;
    if (!Layout_SceneLight_EvaluatePositionAtNormalizedDistance(
            light, path, path ? path->normalized_distance : 0.0f, &position)) {
        position = Layout_SceneLight_EffectivePosition(light, path);
    }
    direction = Vec3_Normalize(Vec3_Sub(light->aim_target, position));
    if (Vec3_Length(direction) < 0.0001f)
        direction = Layout_SceneLight_EffectiveDirection(light, path);
    aim = Layout_SceneLight_AimPoint(light, path);
    reference = fabsf(direction.z) < 0.9f ? (Vec3){0.0f, 0.0f, 1.0f}
                                            : (Vec3){0.0f, 1.0f, 0.0f};
    right = Vec3_Normalize(Vec3_Cross(direction, reference));
    up = Vec3_Normalize(Vec3_Cross(right, direction));
    color = (SDL_Color){
        (Uint8)fminf(255.0f, fmaxf(0.0f, light->color_rgb[0] * 255.0f)),
        (Uint8)fminf(255.0f, fmaxf(0.0f, light->color_rgb[1] * 255.0f)),
        (Uint8)fminf(255.0f, fmaxf(0.0f, light->color_rgb[2] * 255.0f)),
        245
    };
    screen = SceneAuthoringPathHandles_WorldToScreen(state, position, view_ctx);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SceneAuthoringPathHandles_DrawSquareMarker(renderer, (int)screen.x, (int)screen.y,
                                               position_selected ? 5 : 4,
                                               position_selected);

    if (light->kind == LINE_DRAWING_SCENE_LIGHT_POINT) {
        const Vec3 rx = Vec3_Scale(right, fmaxf(0.25f, light->radius));
        const Vec3 uy = Vec3_Scale(up, fmaxf(0.25f, light->radius));
        SceneAuthoringPathHandles_DrawCameraLine(renderer, state, view_ctx,
            Vec3_Sub(position, rx), Vec3_Add(position, rx), color);
        SceneAuthoringPathHandles_DrawCameraLine(renderer, state, view_ctx,
            Vec3_Sub(position, uy), Vec3_Add(position, uy), color);
    } else {
        SceneAuthoringPathHandles_DrawCameraLine(renderer, state, view_ctx,
                                                 position, aim, color);
        if (light->kind == LINE_DRAWING_SCENE_LIGHT_SPOT) {
            const float length = 3.0f;
            const float radius = tanf(light->outer_cone_degrees * 0.01745329252f) * length;
            const Vec3 center = Vec3_Add(position, Vec3_Scale(direction, length));
            const Vec3 rr = Vec3_Scale(right, radius);
            const Vec3 uu = Vec3_Scale(up, radius);
            const Vec3 corners[4] = {
                Vec3_Add(center, Vec3_Add(rr, uu)),
                Vec3_Add(center, Vec3_Sub(rr, uu)),
                Vec3_Sub(center, Vec3_Add(rr, uu)),
                Vec3_Add(center, Vec3_Sub(uu, rr))
            };
            for (int i = 0; i < 4; ++i) {
                SceneAuthoringPathHandles_DrawCameraLine(renderer, state, view_ctx,
                                                         position, corners[i], color);
                SceneAuthoringPathHandles_DrawCameraLine(renderer, state, view_ctx,
                                                         corners[i], corners[(i + 1) % 4], color);
            }
        } else if (light->kind == LINE_DRAWING_SCENE_LIGHT_AREA) {
            const Vec3 rr = Vec3_Scale(right, light->area_size.x * 0.5f);
            const Vec3 uu = Vec3_Scale(up, light->area_size.y * 0.5f);
            const Vec3 corners[4] = {
                Vec3_Add(position, Vec3_Add(rr, uu)),
                Vec3_Add(position, Vec3_Sub(rr, uu)),
                Vec3_Sub(position, Vec3_Add(rr, uu)),
                Vec3_Add(position, Vec3_Sub(uu, rr))
            };
            for (int i = 0; i < 4; ++i) {
                SceneAuthoringPathHandles_DrawCameraLine(renderer, state, view_ctx,
                                                         corners[i], corners[(i + 1) % 4], color);
            }
        }
        screen = SceneAuthoringPathHandles_WorldToScreen(state, aim, view_ctx);
        SDL_SetRenderDrawColor(renderer, aim_hovered ? 255 : color.r,
                              aim_hovered ? 255 : color.g, color.b, 245);
        SceneAuthoringPathHandles_DrawSquareMarker(renderer, (int)screen.x, (int)screen.y,
                                                   aim_selected ? 5 : 4,
                                                   aim_selected || aim_hovered);
        if (aim_selected) {
            SceneAuthoringPathHandles_DrawSelectedGizmo(renderer, state, view_ctx, aim,
                                                        hovered_axis, active_axis);
        }
    }
    if (position_selected &&
        light->position_mode == LINE_DRAWING_SCENE_LIGHT_POSITION_INDEPENDENT) {
        SceneAuthoringPathHandles_DrawSelectedGizmo(renderer, state, view_ctx, position,
                                                    hovered_axis, active_axis);
    }
}

void Render_Editor_SceneAuthoringPathHandles(EditorState* editor, AppContext* ctx) {
    GlobalState* state = Global_Get();
    const LineDrawingSceneAuthoringState* authoring = NULL;
    SpaceViewContext viewCtx = {0};
    size_t light_index = 0u;
    size_t path_index = 0u;
    int hovered_axis = -1;
    int active_axis = -1;
    if (!ctx || !ctx->renderer || !SceneAuthoringPathHandles_ShouldShow(state)) return;
    authoring = &state->layout.sceneAuthoring;
    viewCtx = SpaceAdapter_BuildViewContext(state);
    if (editor && editor->hoveredSceneAuthoringGizmoPart == SCENE_AUTHORING_GIZMO_PART_AXIS) {
        hovered_axis = editor->hoveredSceneAuthoringGizmoAxis == GIZMO_AXIS_DIR_POS_Y ? 1 :
                       editor->hoveredSceneAuthoringGizmoAxis == GIZMO_AXIS_DIR_POS_Z ? 2 : 0;
    }
    if (editor && editor->activeSceneAuthoringGizmoPart == SCENE_AUTHORING_GIZMO_PART_AXIS) {
        active_axis = editor->activeSceneAuthoringGizmoAxis == GIZMO_AXIS_DIR_POS_Y ? 1 :
                      editor->activeSceneAuthoringGizmoAxis == GIZMO_AXIS_DIR_POS_Z ? 2 : 0;
    }

    if (SceneAuthoringPathHandles_SelectedPathIndex(authoring, &path_index)) {
        const LineDrawingScenePath* path = &authoring->paths[path_index];
        const int selected_segment = editor &&
                                     editor->selectedSceneAuthoringPathIndex == (int)path_index &&
                                     editor->selectedSceneAuthoringPathElementKind ==
                                         LINE_DRAWING_SCENE_PATH_ELEMENT_SEGMENT
                                         ? editor->selectedSceneAuthoringPathSegmentIndex : -1;
        const int hovered_segment = editor &&
                                    editor->hoveredSceneAuthoringPathIndex == (int)path_index &&
                                    editor->hoveredSceneAuthoringPathElementKind ==
                                        LINE_DRAWING_SCENE_PATH_ELEMENT_SEGMENT
                                        ? editor->hoveredSceneAuthoringPathSegmentIndex : -1;
        SceneAuthoringPathHandles_DrawPath(ctx->renderer,
                                           state,
                                           &viewCtx,
                                           path,
                                           selected_segment >= 0
                                               ? selected_segment : hovered_segment);
        if (Layout_ScenePathGeometry_IsCompleteCubic(path)) {
            for (size_t anchor = 0u;
                 anchor < Layout_ScenePathEdit_AnchorCount(path);
                 ++anchor) {
                const size_t control = anchor * 3u;
                const Vec2 anchor_screen = SceneAuthoringPathHandles_WorldToScreen(
                    state, path->control_points[control], &viewCtx);
                if (control > 0u) {
                    const Vec2 tangent = SceneAuthoringPathHandles_WorldToScreen(
                        state, path->control_points[control - 1u], &viewCtx);
                    SceneAuthoringPathHandles_DrawLineWithHalo(
                        ctx->renderer, anchor_screen, tangent,
                        (SDL_Color){16, 18, 24, 210}, (SDL_Color){130, 175, 235, 220});
                }
                if (control + 1u < path->control_point_count) {
                    const Vec2 tangent = SceneAuthoringPathHandles_WorldToScreen(
                        state, path->control_points[control + 1u], &viewCtx);
                    SceneAuthoringPathHandles_DrawLineWithHalo(
                        ctx->renderer, anchor_screen, tangent,
                        (SDL_Color){16, 18, 24, 210}, (SDL_Color){130, 175, 235, 220});
                }
            }
        }
        for (size_t i = 0u; i < path->control_point_count; ++i) {
            const LineDrawingScenePathElementRef element =
                Layout_ScenePathEdit_ElementForControl(path, i);
            Vec2 screen = SceneAuthoringPathHandles_WorldToScreen(state,
                                                                  path->control_points[i],
                                                                  &viewCtx);
            bool selected =
                editor &&
                editor->selectedSceneAuthoringPathIndex == (int)path_index &&
                editor->selectedSceneAuthoringControlPointIndex == (int)i;
            const bool hovered = editor &&
                                 editor->hoveredSceneAuthoringPathIndex == (int)path_index &&
                                 editor->hoveredSceneAuthoringControlPointIndex == (int)i;
            const bool tangent = element.kind == LINE_DRAWING_SCENE_PATH_ELEMENT_INCOMING_TANGENT ||
                                 element.kind == LINE_DRAWING_SCENE_PATH_ELEMENT_OUTGOING_TANGENT;
            SDL_SetRenderDrawColor(ctx->renderer,
                                   tangent ? (hovered ? 175 : 125) : 255,
                                   tangent ? (hovered ? 225 : 190)
                                           : (selected || hovered ? 238 : 210),
                                   tangent ? 245 : (selected || hovered ? 125 : 70),
                                   245);
            SceneAuthoringPathHandles_DrawSquareMarker(ctx->renderer,
                                                       (int)screen.x,
                                                       (int)screen.y,
                                                       selected ? 5 : hovered ? 4 : tangent ? 3 : 4,
                                                       selected || hovered);
            if (selected) {
                SceneAuthoringPathHandles_DrawSelectedGizmo(ctx->renderer,
                                                            state,
                                                            &viewCtx,
                                                            path->control_points[i],
                                                            hovered_axis,
                                                            active_axis);
            }
        }
        if (path->role == LINE_DRAWING_SCENE_PATH_ROLE_CAMERA) {
            const LineDrawingSceneCamera* camera =
                Layout_SceneAuthoringState_FindCameraForPathConst(authoring, path);
            if (camera) {
                SceneAuthoringPathHandles_DrawCamera(
                    ctx->renderer, state, &viewCtx, path, camera,
                    editor && editor->selectedSceneAuthoringCameraAim,
                    editor && editor->hoveredSceneAuthoringHandleKind ==
                                  SCENE_AUTHORING_PATH_HANDLE_CAMERA_AIM,
                    hovered_axis, active_axis);
            }
        }
    }

    if (SceneAuthoringPathHandles_SelectedLightIndex(authoring, &light_index)) {
        const LineDrawingSceneLight* light = &authoring->lights[light_index];
        const LineDrawingScenePath* path =
            Layout_SceneAuthoringState_FindPathByIdConst(authoring, light->path_id);
        SceneAuthoringPathHandles_DrawLight(
            ctx->renderer, state, &viewCtx, path, light,
            editor && editor->selectedSceneAuthoringLightPosition,
            editor && editor->selectedSceneAuthoringLightAim,
            editor && editor->hoveredSceneAuthoringHandleKind ==
                          SCENE_AUTHORING_PATH_HANDLE_LIGHT_AIM,
            hovered_axis, active_axis);
    }
}
