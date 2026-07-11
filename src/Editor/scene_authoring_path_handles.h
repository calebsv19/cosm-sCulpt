#pragma once

#include "Core/SDLApp/sdl_app_framework.h"
#include "Core/global_state.h"
#include "Editor/editor.h"

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    SCENE_AUTHORING_PATH_HANDLE_NONE = 0,
    SCENE_AUTHORING_PATH_HANDLE_CONTROL_POINT = 1,
    SCENE_AUTHORING_PATH_HANDLE_LIGHT_POSITION = 2
} SceneAuthoringPathHandleKind;

typedef struct {
    SceneAuthoringPathHandleKind kind;
    size_t light_index;
    size_t path_index;
    size_t control_index;
} SceneAuthoringPathHandleRef;

typedef struct {
    bool active;
    SceneAuthoringPathHandleRef handle;
    bool historyCaptured;
} SceneAuthoringPathHandleDragState;

extern SceneAuthoringPathHandleDragState sceneAuthoringPathHandleDrag;

SceneAuthoringPathHandleRef SceneAuthoringPathHandleRef_None(void);
bool SceneAuthoringPathHandleRef_IsActive(SceneAuthoringPathHandleRef handle);
bool SceneAuthoringPathHandles_ShouldShow(const GlobalState* state);
bool SceneAuthoringPathHandles_Pick(const GlobalState* state,
                                    int mouse_x,
                                    int mouse_y,
                                    SceneAuthoringPathHandleRef* out_handle);
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
void SceneAuthoringPathHandles_Select(EditorState* editor,
                                      SceneAuthoringPathHandleRef handle);
bool BeginSceneAuthoringPathHandleDragSession(GlobalState* state,
                                              EditorState* editor,
                                              SceneAuthoringPathHandleRef handle);
void ResetSceneAuthoringPathHandleDrag(EditorState* editor);
void UpdateSceneAuthoringPathHandleDragPosition(int mouse_x, int mouse_y);
void Render_Editor_SceneAuthoringPathHandles(EditorState* editor, AppContext* ctx);
