#pragma once

#include "Core/global_state.h"
#include "Editor/editor.h"

bool Editor_ObjectFaceSketchArmRectangle(GlobalState* state);
void Editor_ObjectFaceSketchClear(EditorState* editor);
bool Editor_ObjectFaceSketchHasCommittedRectangle(const EditorState* editor);
bool Editor_ObjectFaceSketchIsSelected(const EditorState* editor);
bool Editor_ObjectFaceSketchSelect(EditorState* editor, ObjectFaceSketchHandleKind handle);
void Editor_ObjectFaceSketchDeselect(EditorState* editor);
void Editor_ObjectFaceSketchGetRectangleUV(const EditorState* editor,
                                           Vec2* out_min_uv,
                                           Vec2* out_max_uv);
void Editor_ObjectFaceSketchSetRectangleUV(EditorState* editor,
                                           Vec2 min_uv,
                                           Vec2 max_uv);
bool Editor_ObjectFaceSketchHandleLeftMouseDown(GlobalState* state, int mouse_x, int mouse_y);
void Editor_ObjectFaceSketchHandleMouseMotion(GlobalState* state, int mouse_x, int mouse_y);
void Editor_ObjectFaceSketchHandleLeftMouseUp(GlobalState* state, int mouse_x, int mouse_y);
void Render_EditorObjectFaceSketch(EditorState* editor, AppContext* ctx);
