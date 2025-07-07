// src/Editor/editor.h
#pragma once
#include "Core/SDLApp/sdl_app_framework.h"
#include "Layout/layout.h"
#include "Math/math_util.h"

typedef enum {
    DELETE_MODE_SAFE,      // Only delete selected wall or anchor
    DELETE_MODE_AUTO_PRUNE // Also delete orphan anchors when wall is removed
} DeleteMode;

typedef enum {
    TOOL_IDLE,
    TOOL_PLACING_WALL
} ToolMode;

typedef struct {
    ToolMode mode;
    Vec2 anchor;        // Starting point for wall placement
    bool shiftHeld;     // Whether shift-lock is enabled

    int selectedWallIndex;     // Index of selected wall
    int selectedAnchorIndex;   // Index of selected anchor (-1 if none)

    DeleteMode deleteMode;
} EditorState;

void Editor_Init(EditorState* editor);

// Called when a snapped world point is clicked
void Editor_ClickAt(EditorState* editor, Vec2 worldPos);

// Called by input handler on Shift key down/up
void Editor_SetShiftHeld(EditorState* editor, bool held);

// Renders the ghost wall (anchor → current mouse world pos)
void Editor_Render(EditorState* editor, AppContext* ctx);

