// src/Editor/editor.h
#pragma once
#include "SDLApp/sdl_app_framework.h"
#include "Layout/layout.h"
#include "Math/math_util.h"

typedef enum {
    TOOL_IDLE,
    TOOL_PLACING_WALL
} ToolMode;

typedef struct {
    ToolMode mode;
    Vec2 anchor;     // Starting point for wall
    bool shiftHeld;  // Whether shift-lock is enabled
} EditorState;

void Editor_Init(EditorState* editor);

// Called when a snapped world point is clicked
void Editor_ClickAt(EditorState* editor, Vec2 worldPos);

// Called by input handler on Shift key down/up
void Editor_SetShiftHeld(EditorState* editor, bool held);

// Renders the ghost wall (anchor → current mouse world pos)
void Editor_Render(EditorState* editor, AppContext* ctx);

