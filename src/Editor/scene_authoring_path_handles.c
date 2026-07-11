#include "Editor/scene_authoring_path_handles.h"

#include "Core/space_mode_adapter.h"
#include "Layout/Grid/grid.h"

#include <SDL2/SDL.h>
#include <math.h>
#include <string.h>

SceneAuthoringPathHandleDragState sceneAuthoringPathHandleDrag = {
    .active = false,
    .handle = {
        .kind = SCENE_AUTHORING_PATH_HANDLE_NONE,
        .light_index = 0u,
        .path_index = 0u,
        .control_index = 0u
    },
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
                                    SceneAuthoringPathHandleRef* out_handle) {
    const LineDrawingSceneAuthoringState* authoring = NULL;
    SpaceViewContext viewCtx = {0};
    SceneAuthoringPathHandleRef best = SceneAuthoringPathHandleRef_None();
    float best_dist = 999999.0f;
    size_t light_index = 0u;
    size_t path_index = 0u;
    const float pick_radius = 12.0f;

    if (out_handle) *out_handle = best;
    if (!SceneAuthoringPathHandles_ShouldShow(state)) return false;
    authoring = &state->layout.sceneAuthoring;
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
    if (out_handle) *out_handle = best;
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
                                              SceneAuthoringPathHandleRef handle) {
    if (!state || !editor || !SceneAuthoringPathHandleRef_IsActive(handle)) return false;
    if (!SceneAuthoringPathHandles_ShouldShow(state)) return false;
    SceneAuthoringPathHandles_Select(editor, handle);
    sceneAuthoringPathHandleDrag.active = true;
    sceneAuthoringPathHandleDrag.handle = handle;
    sceneAuthoringPathHandleDrag.historyCaptured = false;
    editor->isPreciseDrag = (SDL_GetModState() & KMOD_ALT) != 0;
    return true;
}

void ResetSceneAuthoringPathHandleDrag(EditorState* editor) {
    sceneAuthoringPathHandleDrag.active = false;
    sceneAuthoringPathHandleDrag.handle = SceneAuthoringPathHandleRef_None();
    sceneAuthoringPathHandleDrag.historyCaptured = false;
    if (editor) editor->isPreciseDrag = false;
}

void UpdateSceneAuthoringPathHandleDragPosition(int mouse_x, int mouse_y) {
    GlobalState* state = Global_Get();
    SpaceViewContext viewCtx = {0};
    Vec3 world = {0};
    bool precise = false;
    if (!state || !sceneAuthoringPathHandleDrag.active) return;
    if (!SceneAuthoringPathHandleRef_IsActive(sceneAuthoringPathHandleDrag.handle)) return;
    precise = (SDL_GetModState() & KMOD_ALT) != 0;
    state->editor.isPreciseDrag = precise;
    viewCtx = SpaceAdapter_BuildViewContext(state);
    if (!SpaceAdapter_ScreenToWorld(mouse_x,
                                    mouse_y,
                                    &state->grid,
                                    &viewCtx,
                                    !precise,
                                    &world)) {
        return;
    }
    if (!sceneAuthoringPathHandleDrag.historyCaptured) {
        Editor_HistoryCapture(&state->editor, &state->layout);
        sceneAuthoringPathHandleDrag.historyCaptured = true;
    }
    if (SceneAuthoringPathHandles_SetWorldPoint(state,
                                                sceneAuthoringPathHandleDrag.handle,
                                                world)) {
        Global_FlagLayoutChanged();
    }
}

static void SceneAuthoringPathHandles_DrawFilledCircle(SDL_Renderer* renderer,
                                                       int cx,
                                                       int cy,
                                                       int radius) {
    if (!renderer || radius <= 0) return;
    int r2 = radius * radius;
    for (int dy = -radius; dy <= radius; ++dy) {
        int dx = (int)sqrtf((float)(r2 - dy * dy));
        (void)SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

static void SceneAuthoringPathHandles_DrawPath(SDL_Renderer* renderer,
                                               const GlobalState* state,
                                               const SpaceViewContext* viewCtx,
                                               const LineDrawingSceneCameraPath* path) {
    if (!renderer || !state || !viewCtx || !path) return;
    if (path->control_point_count < 2u) return;
    SDL_SetRenderDrawColor(renderer, 245, 205, 110, 215);
    for (size_t i = 1u; i < path->control_point_count; ++i) {
        Vec2 a = SceneAuthoringPathHandles_WorldToScreen(state,
                                                         path->control_points[i - 1u],
                                                         viewCtx);
        Vec2 b = SceneAuthoringPathHandles_WorldToScreen(state,
                                                         path->control_points[i],
                                                         viewCtx);
        (void)SDL_RenderDrawLine(renderer, (int)a.x, (int)a.y, (int)b.x, (int)b.y);
    }
}

void Render_Editor_SceneAuthoringPathHandles(EditorState* editor, AppContext* ctx) {
    GlobalState* state = Global_Get();
    const LineDrawingSceneAuthoringState* authoring = NULL;
    SpaceViewContext viewCtx = {0};
    size_t light_index = 0u;
    size_t path_index = 0u;
    (void)editor;
    if (!ctx || !ctx->renderer || !SceneAuthoringPathHandles_ShouldShow(state)) return;
    authoring = &state->layout.sceneAuthoring;
    viewCtx = SpaceAdapter_BuildViewContext(state);

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
            SDL_SetRenderDrawColor(ctx->renderer, 245, 205, 110, 235);
            SceneAuthoringPathHandles_DrawFilledCircle(ctx->renderer,
                                                       (int)screen.x,
                                                       (int)screen.y,
                                                       selected
                                                           ? 8
                                                           : (i == 0u ||
                                                              i + 1u == path->control_point_count
                                                                  ? 6
                                                                  : 5));
            SDL_SetRenderDrawColor(ctx->renderer, 70, 45, 10, 240);
            (void)SDL_RenderDrawPoint(ctx->renderer, (int)screen.x, (int)screen.y);
        }
    }

    if (SceneAuthoringPathHandles_SelectedLightIndex(authoring, &light_index)) {
        const LineDrawingSceneLight* light = &authoring->lights[light_index];
        Vec2 screen = SceneAuthoringPathHandles_WorldToScreen(state, light->position, &viewCtx);
        SDL_SetRenderDrawColor(ctx->renderer, 255, 245, 160, 245);
        SceneAuthoringPathHandles_DrawFilledCircle(ctx->renderer,
                                                   (int)screen.x,
                                                   (int)screen.y,
                                                   editor &&
                                                   editor->selectedSceneAuthoringLightPosition
                                                       ? 9
                                                       : 7);
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
    }
}
