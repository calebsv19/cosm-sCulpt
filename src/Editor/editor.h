// src/Editor/editor.h
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "Core/SDLApp/sdl_app_framework.h"
#include "Layout/layout.h"
#include "Math/math_util.h"
#include "Editor/space_gizmo_drag.h"

typedef enum {
    DELETE_MODE_SAFE,      // Only delete selected wall or anchor
    DELETE_MODE_AUTO_PRUNE // Also delete orphan anchors when wall is removed
} DeleteMode;

typedef enum {
    TOOL_IDLE,
    TOOL_PLACING_WALL
} ToolMode;

typedef struct {
    char** entries;
    size_t count;
    size_t capacity;
} EditorHistoryStack;

typedef struct {
    int anchorIndex;
    Vec3 startPos;
} AnchorDragSnapshot;

typedef struct {
    ToolMode mode;
    Vec3 anchor;        // Starting point for wall placement
    bool shiftHeld;     // Whether shift-lock is enabled

    bool selectionBoxActive;
    bool selectionBoxAdditive;
    Vec2 selectionBoxStart;
    Vec2 selectionBoxEnd;

    int selectedWallIndex;     // Index of selected wall
    int selectedAnchorIndex;   // Index of selected anchor (-1 if none)
    int hoveredWallIndex;
    int hoveredAnchorIndex;
    int selectedHandleAnchor;
    int selectedHandleComponent;   // 0 = incoming, 1 = outgoing, -1 = none
    uint32_t selectedObject3DId;
    int selectedObject3DResizeHandle; // PlaneResizeHandleKind or PLANE_RESIZE_HANDLE_NONE
    int selectedObject3DPrismHandle;  // RectPrismResizeHandleKind or RECT_PRISM_RESIZE_HANDLE_NONE
    int hoveredHandleAnchor;
    int hoveredHandleComponent;
    int hoveredGizmoAxis;          // GizmoAxisDirection or -1
    int hoveredObject3DGizmoAxis;  // RectPrismAxisDirection or -1
    int activeObject3DGizmoAxis;   // RectPrismAxisDirection or -1 while dragging
    uint32_t hoveredObject3DId;
    int hoveredObject3DResizeHandle; // PlaneResizeHandleKind or PLANE_RESIZE_HANDLE_NONE
    int hoveredObject3DPrismHandle;  // RectPrismResizeHandleKind or RECT_PRISM_RESIZE_HANDLE_NONE

    DeleteMode deleteMode;

    int* anchorSelection;
    int anchorSelectionCount;
    int anchorSelectionCapacity;

    AnchorDragSnapshot* dragSnapshots;
    int dragSnapshotCount;
    int dragSnapshotCapacity;
    bool isDraggingAnchor;
    bool isResizingObject3D;
    bool isRotatingObject3D;
    bool object3DRotateMode;
    bool isPreciseDrag;
    GizmoAxisDragSession gizmoDrag;

    EditorHistoryStack undoStack;
    EditorHistoryStack redoStack;
} EditorState;

#define EDITOR_HISTORY_MAX 64

void Editor_Init(EditorState* editor);
void Editor_Free(EditorState* editor);

// Called when a snapped world point is clicked
void Editor_ClickAt(EditorState* editor, Vec3 worldPos);

// Called by input handler on Shift key down/up
void Editor_SetShiftHeld(EditorState* editor, bool held);

void Editor_HistoryCapture(EditorState* editor, const Layout* layout);
bool Editor_Undo(EditorState* editor, Layout* layout);
bool Editor_Redo(EditorState* editor, Layout* layout);
size_t Editor_UndoCount(const EditorState* editor);
size_t Editor_RedoCount(const EditorState* editor);
void Editor_ClearHistory(EditorState* editor);

void Editor_ClearAnchorSelection(EditorState* editor);
void Editor_SelectAnchor(EditorState* editor, int anchorIndex, bool additive);
bool Editor_IsAnchorSelected(const EditorState* editor, int anchorIndex);
int Editor_SelectedAnchorCount(const EditorState* editor);
void Editor_BeginAnchorDrag(EditorState* editor, const Layout* layout);
void Editor_UpdateAnchorDrag(EditorState* editor, Layout* layout, Vec3 primaryNewPos);
void Editor_EndAnchorDrag(EditorState* editor);
void Editor_ResetGizmoDrag(EditorState* editor);
void Editor_SelectAnchorsInBox(EditorState* editor, const Layout* layout, Vec2 min, Vec2 max, bool additive);

// Renders the ghost wall (anchor → current mouse world pos)
void Editor_Render(EditorState* editor, AppContext* ctx);
