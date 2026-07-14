#pragma once

#include "Core/SDLApp/sdl_app_framework.h"
#include "Core/global_state.h"
#include "Editor/editor.h"
#include "Editor/space_gizmo_drag.h"
#include "Layout/scene/layout_scene_path_edit.h"

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    SCENE_AUTHORING_PATH_HANDLE_NONE = 0,
    SCENE_AUTHORING_PATH_HANDLE_CONTROL_POINT = 1,
    SCENE_AUTHORING_PATH_HANDLE_LIGHT_POSITION = 2,
    SCENE_AUTHORING_PATH_HANDLE_CAMERA_AIM = 3,
    SCENE_AUTHORING_PATH_HANDLE_LIGHT_AIM = 4
} SceneAuthoringPathHandleKind;

typedef struct {
    SceneAuthoringPathHandleKind kind;
    size_t light_index;
    size_t camera_index;
    size_t path_index;
    size_t control_index;
    size_t segment_index;
    LineDrawingScenePathElementKind element_kind;
} SceneAuthoringPathHandleRef;

typedef enum {
    SCENE_AUTHORING_GIZMO_PART_NONE = 0,
    SCENE_AUTHORING_GIZMO_PART_CENTER = 1,
    SCENE_AUTHORING_GIZMO_PART_AXIS = 2
} SceneAuthoringGizmoPart;

typedef struct {
    SceneAuthoringPathHandleRef handle;
    SceneAuthoringGizmoPart part;
    GizmoAxisDirection axis;
} SceneAuthoringGizmoPickResult;

typedef struct {
    bool active;
    SceneAuthoringGizmoPickResult pick;
    Vec2 mouseStartScreen;
    Vec3 startWorld;
    Vec2 projectedAxisVector;
    float worldUnitsPerPixel;
    bool historyCaptured;
} SceneAuthoringPathHandleDragState;

extern SceneAuthoringPathHandleDragState sceneAuthoringPathHandleDrag;

SceneAuthoringPathHandleRef SceneAuthoringPathHandleRef_None(void);
bool SceneAuthoringPathHandleRef_IsActive(SceneAuthoringPathHandleRef handle);
SceneAuthoringGizmoPickResult SceneAuthoringGizmoPickResult_None(void);
bool SceneAuthoringGizmoPickResult_IsActive(SceneAuthoringGizmoPickResult pick);
bool SceneAuthoringPathHandles_ShouldShow(const GlobalState* state);
bool SceneAuthoringPathHandles_Pick(const GlobalState* state,
                                    int mouse_x,
                                    int mouse_y,
                                    SceneAuthoringGizmoPickResult* out_pick);
bool SceneAuthoringPathHandles_InsertControlPointAtScreen(GlobalState* state,
                                                          EditorState* editor,
                                                          int mouse_x,
                                                          int mouse_y,
                                                          SceneAuthoringPathHandleRef* out_handle);
bool SceneAuthoringPathHandles_DeleteSelectedControlPoint(GlobalState* state,
                                                          EditorState* editor);
bool SceneAuthoringPathHandles_SetWorldPoint(GlobalState* state,
                                             SceneAuthoringPathHandleRef handle,
                                             Vec3 point);
bool SceneAuthoringPathHandles_CycleSelectedTangentMode(GlobalState* state,
                                                        EditorState* editor);
void SceneAuthoringPathHandles_Select(EditorState* editor,
                                      SceneAuthoringPathHandleRef handle);
bool BeginSceneAuthoringPathHandleDragSession(GlobalState* state,
                                              EditorState* editor,
                                              SceneAuthoringGizmoPickResult pick,
                                              int mouse_x,
                                              int mouse_y);
void ResetSceneAuthoringPathHandleDrag(EditorState* editor);
void UpdateSceneAuthoringPathHandleDragPosition(int mouse_x, int mouse_y);
void Render_Editor_SceneAuthoringPathHandles(EditorState* editor, AppContext* ctx);
