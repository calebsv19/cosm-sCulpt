#pragma once

#include "Core/global_state.h"
#include "Editor/editor.h"

bool Editor_ObjectFaceSketchBeginEditDrag(GlobalState* state,
                                          ObjectFaceSketchHandleKind handle,
                                          int mouse_x,
                                          int mouse_y);
void Editor_ObjectFaceSketchUpdateEditDrag(GlobalState* state, int mouse_x, int mouse_y);
void Editor_ObjectFaceSketchEndEditDrag(GlobalState* state, int mouse_x, int mouse_y);
