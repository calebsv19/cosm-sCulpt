#pragma once

#include "Core/SDLApp/sdl_app_framework.h"
#include "Core/global_state.h"
#include "Editor/editor.h"

void ResetObjectResizeDrag(EditorState* editor);
void ResetObjectGizmoDrag(EditorState* editor);
void ResetObjectTranslateDrag(EditorState* editor);
void ResetObjectRotateDrag(EditorState* editor);

bool BeginGizmoDragSession(GlobalState* state,
                           EditorState* editor,
                           int anchorIndex,
                           GizmoAxisDirection axis,
                           int mouseX,
                           int mouseY);
bool BeginObjectResizeDragSession(GlobalState* state,
                                  EditorState* editor,
                                  uint32_t objectId,
                                  PlaneResizeHandleKind handle);
bool BeginObjectGizmoDragSession(GlobalState* state,
                                 EditorState* editor,
                                 uint32_t objectId,
                                 RectPrismResizeHandleKind handle,
                                 RectPrismAxisDirection axisDirection,
                                 int mouseX,
                                 int mouseY);
bool BeginObjectTranslateDragSession(GlobalState* state,
                                     EditorState* editor,
                                     uint32_t objectId,
                                     GizmoAxisDirection axis,
                                     int mouseX,
                                     int mouseY);
bool BeginObjectRotateDragSession(GlobalState* state,
                                  EditorState* editor,
                                  uint32_t objectId,
                                  GizmoAxisDirection axis,
                                  int mouseX,
                                  int mouseY);

void HandleMouseDrag(const SDL_MouseMotionEvent* motion);
