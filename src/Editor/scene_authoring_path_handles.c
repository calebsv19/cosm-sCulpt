#include "Editor/scene_authoring_path_handles.h"

#include "Core/space_mode_adapter.h"
#include "Layout/Grid/grid.h"

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
        .path_index = 0u,
        .control_index = 0u
    };
}

bool SceneAuthoringPathHandleRef_IsActive(SceneAuthoringPathHandleRef handle) {
    return handle.kind == SCENE_AUTHORING_PATH_HANDLE_CONTROL_POINT ||
           handle.kind == SCENE_AUTHORING_PATH_HANDLE_LIGHT_POSITION;
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
    if (authoring->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_CAMERA_PATH) {
        if (authoring->selected_index >= authoring->camera_path_count) return false;
        if (out_path_index) *out_path_index = authoring->selected_index;
        return true;
    }
    if (authoring->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT) {
        const LineDrawingSceneLight* light = NULL;
        if (authoring->selected_index >= authoring->light_count) return false;
        light = &authoring->lights[authoring->selected_index];
        if (light->path_id[0] == '\0') return false;
        for (size_t i = 0u; i < authoring->camera_path_count; ++i) {
            if (strncmp(authoring->camera_paths[i].path_id,
                        light->path_id,
                        sizeof(authoring->camera_paths[i].path_id)) == 0) {
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
    if (authoring->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_CAMERA_PATH) {
        return authoring->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_CAMERA_PATH &&
               authoring->selected_index < authoring->camera_path_count;
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
        const LineDrawingSceneCameraPath* path = NULL;
        if (handle.path_index >= authoring->camera_path_count) return false;
        path = &authoring->camera_paths[handle.path_index];
        if (handle.control_index >= path->control_point_count) return false;
        *out_point = path->control_points[handle.control_index];
        return true;
    }
    if (handle.kind == SCENE_AUTHORING_PATH_HANDLE_LIGHT_POSITION) {
        if (handle.light_index >= authoring->light_count) return false;
        *out_point = authoring->lights[handle.light_index].position;
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
        if (path_index >= authoring->camera_path_count) return false;
        if (control_index >= authoring->camera_paths[path_index].control_point_count) return false;
        handle = (SceneAuthoringPathHandleRef){
            .kind = SCENE_AUTHORING_PATH_HANDLE_CONTROL_POINT,
            .light_index = 0u,
            .path_index = path_index,
            .control_index = control_index
        };
        if (out_handle) *out_handle = handle;
        return true;
    }
    if (editor->selectedSceneAuthoringLightPosition &&
        authoring->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT &&
        authoring->selected_index < authoring->light_count) {
        handle = (SceneAuthoringPathHandleRef){
            .kind = SCENE_AUTHORING_PATH_HANDLE_LIGHT_POSITION,
            .light_index = authoring->selected_index,
            .path_index = 0u,
            .control_index = 0u
        };
        if (out_handle) *out_handle = handle;
        return true;
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
    editor->selectedSceneAuthoringLightPosition = false;
    if (handle.kind == SCENE_AUTHORING_PATH_HANDLE_CONTROL_POINT) {
        editor->selectedSceneAuthoringPathIndex = (int)handle.path_index;
        editor->selectedSceneAuthoringControlPointIndex = (int)handle.control_index;
    } else if (handle.kind == SCENE_AUTHORING_PATH_HANDLE_LIGHT_POSITION) {
        editor->selectedSceneAuthoringLightPosition = true;
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
            }
        }
    }

    if (SceneAuthoringPathHandles_SelectedPathIndex(authoring, &path_index)) {
        const LineDrawingSceneCameraPath* path = &authoring->camera_paths[path_index];
        for (size_t i = 0u; i < path->control_point_count; ++i) {
            SceneAuthoringPathHandleRef candidate = {
                .kind = SCENE_AUTHORING_PATH_HANDLE_CONTROL_POINT,
                .light_index = light_index,
                .path_index = path_index,
                .control_index = i
            };
            Vec2 screen =
                SceneAuthoringPathHandles_WorldToScreen(state, path->control_points[i], &viewCtx);
            float dist = Vec2_Distance(screen, (Vec2){ (float)mouse_x, (float)mouse_y });
            if (dist < best_dist && dist <= pick_radius) {
                best = candidate;
                best_dist = dist;
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

static size_t SceneAuthoringPathHandles_InsertIndexForScreenPoint(
    const GlobalState* state,
    const LineDrawingSceneCameraPath* path,
    const SpaceViewContext* viewCtx,
    int mouse_x,
    int mouse_y) {
    Vec2 mouse = { (float)mouse_x, (float)mouse_y };
    size_t insert_index = path ? path->control_point_count : 0u;
    float best_dist = 999999.0f;
    if (!state || !path || !viewCtx || path->control_point_count < 2u) return insert_index;
    for (size_t i = 1u; i < path->control_point_count; ++i) {
        Vec2 a = SceneAuthoringPathHandles_WorldToScreen(state,
                                                         path->control_points[i - 1u],
                                                         viewCtx);
        Vec2 b = SceneAuthoringPathHandles_WorldToScreen(state,
                                                         path->control_points[i],
                                                         viewCtx);
        float dist = SceneAuthoringPathHandles_DistancePointToSegment(mouse, a, b);
        if (dist < best_dist) {
            best_dist = dist;
            insert_index = i;
        }
    }
    return insert_index;
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
    SceneAuthoringPathHandleRef inserted = SceneAuthoringPathHandleRef_None();
    if (out_handle) *out_handle = inserted;
    if (!state || !editor || !SceneAuthoringPathHandles_ShouldShow(state)) return false;
    authoring = &state->layout.sceneAuthoring;
    if (!SceneAuthoringPathHandles_SelectedPathIndex(authoring, &path_index)) return false;
    if (path_index >= authoring->camera_path_count) return false;
    if (authoring->camera_paths[path_index].control_point_count >=
        (sizeof(authoring->camera_paths[path_index].control_points) /
         sizeof(authoring->camera_paths[path_index].control_points[0]))) {
        return false;
    }
    viewCtx = SpaceAdapter_BuildViewContext(state);
    if (!SpaceAdapter_ScreenToWorld(mouse_x,
                                    mouse_y,
                                    &state->grid,
                                    &viewCtx,
                                    true,
                                    &world)) {
        return false;
    }
    insert_index = SceneAuthoringPathHandles_InsertIndexForScreenPoint(
        state,
        &authoring->camera_paths[path_index],
        &viewCtx,
        mouse_x,
        mouse_y);
    Editor_HistoryCapture(editor, &state->layout);
    if (!Layout_SceneAuthoringState_InsertCameraPathControlPoint(authoring,
                                                                 path_index,
                                                                 insert_index,
                                                                 world)) {
        return false;
    }
    inserted = (SceneAuthoringPathHandleRef){
        .kind = SCENE_AUTHORING_PATH_HANDLE_CONTROL_POINT,
        .light_index = 0u,
        .path_index = path_index,
        .control_index = insert_index
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
        editor->selectedSceneAuthoringControlPointIndex < 0) {
        return false;
    }
    path_index = (size_t)editor->selectedSceneAuthoringPathIndex;
    control_index = (size_t)editor->selectedSceneAuthoringControlPointIndex;
    authoring = &state->layout.sceneAuthoring;
    if (path_index >= authoring->camera_path_count ||
        control_index >= authoring->camera_paths[path_index].control_point_count ||
        authoring->camera_paths[path_index].control_point_count <= 2u) {
        return false;
    }
    Editor_HistoryCapture(editor, &state->layout);
    if (!Layout_SceneAuthoringState_DeleteCameraPathControlPoint(authoring,
                                                                 path_index,
                                                                 control_index)) {
        return false;
    }
    if (path_index < authoring->camera_path_count) {
        size_t count = authoring->camera_paths[path_index].control_point_count;
        if (count > 0u) {
            if (control_index >= count) control_index = count - 1u;
            editor->selectedSceneAuthoringPathIndex = (int)path_index;
            editor->selectedSceneAuthoringControlPointIndex = (int)control_index;
        } else {
            editor->selectedSceneAuthoringPathIndex = -1;
            editor->selectedSceneAuthoringControlPointIndex = -1;
        }
    }
    editor->selectedSceneAuthoringLightPosition = false;
    Global_FlagLayoutChanged();
    return true;
}

bool SceneAuthoringPathHandles_SetWorldPoint(GlobalState* state,
                                             SceneAuthoringPathHandleRef handle,
                                             Vec3 point) {
    if (!state) return false;
    if (handle.kind == SCENE_AUTHORING_PATH_HANDLE_CONTROL_POINT) {
        return Layout_SceneAuthoringState_SetCameraPathControlPoint(&state->layout.sceneAuthoring,
                                                                    handle.path_index,
                                                                    handle.control_index,
                                                                    point);
    }
    if (handle.kind == SCENE_AUTHORING_PATH_HANDLE_LIGHT_POSITION) {
        return Layout_SceneAuthoringState_SetLightPosition(&state->layout.sceneAuthoring,
                                                           handle.light_index,
                                                           point);
    }
    return false;
}

bool BeginSceneAuthoringPathHandleDragSession(GlobalState* state,
                                              EditorState* editor,
                                              SceneAuthoringGizmoPickResult pick,
                                              int mouse_x,
                                              int mouse_y) {
    Vec3 start_world = {0};
    if (!state || !editor || !SceneAuthoringGizmoPickResult_IsActive(pick)) return false;
    if (!SceneAuthoringPathHandles_ShouldShow(state)) return false;
    if (!SceneAuthoringPathHandles_PointForHandle(state, pick.handle, &start_world)) return false;
    SceneAuthoringPathHandles_Select(editor, pick.handle);
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
                                               const LineDrawingSceneCameraPath* path) {
    if (!renderer || !state || !viewCtx || !path) return;
    if (path->control_point_count < 2u) return;
    for (size_t i = 1u; i < path->control_point_count; ++i) {
        Vec2 a = SceneAuthoringPathHandles_WorldToScreen(state,
                                                         path->control_points[i - 1u],
                                                         viewCtx);
        Vec2 b = SceneAuthoringPathHandles_WorldToScreen(state,
                                                         path->control_points[i],
                                                         viewCtx);
        SceneAuthoringPathHandles_DrawLineWithHalo(renderer,
                                                   a,
                                                   b,
                                                   (SDL_Color){ 38, 26, 8, 220 },
                                                   (SDL_Color){ 255, 218, 80, 245 });
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
        const LineDrawingSceneCameraPath* path = &authoring->camera_paths[path_index];
        SceneAuthoringPathHandles_DrawPath(ctx->renderer, state, &viewCtx, path);
        for (size_t i = 0u; i < path->control_point_count; ++i) {
            Vec2 screen = SceneAuthoringPathHandles_WorldToScreen(state,
                                                                  path->control_points[i],
                                                                  &viewCtx);
            bool selected =
                editor &&
                editor->selectedSceneAuthoringPathIndex == (int)path_index &&
                editor->selectedSceneAuthoringControlPointIndex == (int)i;
            SDL_SetRenderDrawColor(ctx->renderer,
                                   255,
                                   selected ? 238 : 210,
                                   selected ? 125 : 70,
                                   245);
            SceneAuthoringPathHandles_DrawSquareMarker(ctx->renderer,
                                                       (int)screen.x,
                                                       (int)screen.y,
                                                       selected ? 5 : 3,
                                                       selected);
            if (selected) {
                SceneAuthoringPathHandles_DrawSelectedGizmo(ctx->renderer,
                                                            state,
                                                            &viewCtx,
                                                            path->control_points[i],
                                                            hovered_axis,
                                                            active_axis);
            }
        }
    }

    if (SceneAuthoringPathHandles_SelectedLightIndex(authoring, &light_index)) {
        const LineDrawingSceneLight* light = &authoring->lights[light_index];
        Vec2 screen = SceneAuthoringPathHandles_WorldToScreen(state, light->position, &viewCtx);
        SDL_SetRenderDrawColor(ctx->renderer, 255, 245, 160, 245);
        SceneAuthoringPathHandles_DrawSquareMarker(ctx->renderer,
                                                   (int)screen.x,
                                                   (int)screen.y,
                                                   editor && editor->selectedSceneAuthoringLightPosition
                                                       ? 5
                                                       : 4,
                                                   editor && editor->selectedSceneAuthoringLightPosition);
        SDL_SetRenderDrawColor(ctx->renderer, 70, 60, 10, 240);
        (void)SDL_RenderDrawLine(ctx->renderer,
                                 (int)screen.x - 8,
                                 (int)screen.y,
                                 (int)screen.x + 8,
                                 (int)screen.y);
        (void)SDL_RenderDrawLine(ctx->renderer,
                                 (int)screen.x,
                                 (int)screen.y - 8,
                                 (int)screen.x,
                                 (int)screen.y + 8);
        if (editor && editor->selectedSceneAuthoringLightPosition) {
            SceneAuthoringPathHandles_DrawSelectedGizmo(ctx->renderer,
                                                        state,
                                                        &viewCtx,
                                                        light->position,
                                                        hovered_axis,
                                                        active_axis);
        }
    }
}
