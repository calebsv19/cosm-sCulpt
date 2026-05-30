#pragma once

#include "Core/global_state.h"
#include "Editor/editor.h"

#include <stdbool.h>

bool Editor_ObjectFaceExtrudeArm(GlobalState* state, ObjectFaceExtrudeMode mode);
bool Editor_ObjectFaceExtrudeTrigger(GlobalState* state, ObjectFaceExtrudeMode mode);
void Editor_ObjectFaceExtrudeClear(EditorState* editor);
bool Editor_ObjectFaceExtrudeHandleLeftMouseDown(GlobalState* state, int mouse_x, int mouse_y);
void Editor_ObjectFaceExtrudeHandleMouseMotion(GlobalState* state, int mouse_x, int mouse_y);
void Editor_ObjectFaceExtrudeHandleLeftMouseUp(GlobalState* state, int mouse_x, int mouse_y);
void Render_EditorObjectFaceExtrude(EditorState* editor, AppContext* ctx);
