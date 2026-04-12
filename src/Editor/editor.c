// src/Editor/editor.c
#include "Editor/editor.h"
#include "Core/global_state.h"
#include "Core/space_mode_adapter.h"
#include "Math/math_util.h"
#include "Layout/layout.h"
#include "Layout/layout_json.h"
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void HistoryStack_Init(EditorHistoryStack* stack) {
    stack->entries = NULL;
    stack->count = 0;
    stack->capacity = 0;
}

static void HistoryStack_Reset(EditorHistoryStack* stack) {
    for (size_t i = 0; i < stack->count; ++i) {
        free(stack->entries[i]);
    }
    stack->count = 0;
}

static void HistoryStack_Free(EditorHistoryStack* stack) {
    HistoryStack_Reset(stack);
    free(stack->entries);
    stack->entries = NULL;
    stack->capacity = 0;
}

static void HistoryStack_EnsureCapacity(EditorHistoryStack* stack, size_t desired) {
    if (stack->capacity >= desired) return;
    size_t newCap = stack->capacity ? stack->capacity * 2 : 8;
    while (newCap < desired) newCap *= 2;
    char** newEntries = realloc(stack->entries, newCap * sizeof(char*));
    if (!newEntries) return;
    stack->entries = newEntries;
    stack->capacity = newCap;
}

static void HistoryStack_Push(EditorHistoryStack* stack, char* snapshot) {
    if (!snapshot) return;
    HistoryStack_EnsureCapacity(stack, stack->count + 1);
    if (!stack->entries) {
        free(snapshot);
        return;
    }

    stack->entries[stack->count++] = snapshot;

    if (stack->count > EDITOR_HISTORY_MAX) {
        free(stack->entries[0]);
        memmove(&stack->entries[0], &stack->entries[1], (stack->count - 1) * sizeof(char*));
        stack->count--;
    }
}

static char* HistoryStack_Pop(EditorHistoryStack* stack) {
    if (stack->count == 0) return NULL;
    return stack->entries[--stack->count];
}

static void AnchorSelection_EnsureCapacity(EditorState* editor, int desired) {
    if (editor->anchorSelectionCapacity >= desired) return;
    int newCap = editor->anchorSelectionCapacity ? editor->anchorSelectionCapacity * 2 : 4;
    while (newCap < desired) newCap *= 2;
    int* newList = realloc(editor->anchorSelection, newCap * sizeof(int));
    if (!newList) return;
    editor->anchorSelection = newList;
    editor->anchorSelectionCapacity = newCap;
}

static bool AnchorSelection_Contains(const EditorState* editor, int anchorIndex) {
    for (int i = 0; i < editor->anchorSelectionCount; ++i) {
        if (editor->anchorSelection[i] == anchorIndex) return true;
    }
    return false;
}

static void AnchorSelection_Add(EditorState* editor, int anchorIndex) {
    if (anchorIndex < 0) return;
    if (AnchorSelection_Contains(editor, anchorIndex)) return;
    AnchorSelection_EnsureCapacity(editor, editor->anchorSelectionCount + 1);
    if (!editor->anchorSelection) return;
    editor->anchorSelection[editor->anchorSelectionCount++] = anchorIndex;
}

static void AnchorSelection_Clear(EditorState* editor) {
    editor->anchorSelectionCount = 0;
}

static void DragSnapshot_EnsureCapacity(EditorState* editor, int desired) {
    if (editor->dragSnapshotCapacity >= desired) return;
    int newCap = editor->dragSnapshotCapacity ? editor->dragSnapshotCapacity * 2 : 4;
    while (newCap < desired) newCap *= 2;
    AnchorDragSnapshot* newList = realloc(editor->dragSnapshots,
                                          newCap * sizeof(AnchorDragSnapshot));
    if (!newList) return;
    editor->dragSnapshots = newList;
    editor->dragSnapshotCapacity = newCap;
}

void Editor_ResetGizmoDrag(EditorState* editor) {
    if (!editor) return;
    editor->gizmoDrag.active = false;
    editor->gizmoDrag.axis = GIZMO_AXIS_DIR_POS_X;
    editor->gizmoDrag.anchorIndex = -1;
    editor->gizmoDrag.mouseStartScreen = (Vec2){ 0.0f, 0.0f };
    editor->gizmoDrag.primaryStartWorld = (Vec3){ 0.0f, 0.0f, 0.0f };
    editor->gizmoDrag.worldUnitsPerPixel = 0.0f;
    editor->gizmoDrag.smooth = false;
}

void Editor_Init(EditorState* editor) {
    editor->mode = TOOL_IDLE;
    editor->shiftHeld = false;
    editor->anchor = (Vec3){0, 0, 0};
    editor->selectionBoxActive = false;
    editor->selectionBoxAdditive = false;
    editor->selectionBoxStart = (Vec2){0, 0};
    editor->selectionBoxEnd = (Vec2){0, 0};
    editor->selectedWallIndex = -1;
    editor->selectedAnchorIndex = -1;
    editor->hoveredWallIndex = -1;
    editor->hoveredAnchorIndex = -1;
    editor->selectedHandleAnchor = -1;
    editor->selectedHandleComponent = -1;
    editor->selectedObject3DId = 0u;
    editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    editor->hoveredHandleAnchor = -1;
    editor->hoveredHandleComponent = -1;
    editor->hoveredGizmoAxis = -1;
    editor->hoveredObject3DGizmoAxis = -1;
    editor->activeObject3DGizmoAxis = -1;
    editor->hoveredObject3DId = 0u;
    editor->hoveredObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    editor->hoveredObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    editor->deleteMode = DELETE_MODE_SAFE;
    editor->isDraggingAnchor = false;
    editor->isResizingObject3D = false;
    editor->isRotatingObject3D = false;
    editor->object3DRotateMode = false;
    editor->isPreciseDrag = false;
    editor->anchorSelection = NULL;
    editor->anchorSelectionCount = 0;
    editor->anchorSelectionCapacity = 0;
    editor->dragSnapshots = NULL;
    editor->dragSnapshotCount = 0;
    editor->dragSnapshotCapacity = 0;
    Editor_ResetGizmoDrag(editor);
    HistoryStack_Init(&editor->undoStack);
    HistoryStack_Init(&editor->redoStack);
}

void Editor_Free(EditorState* editor) {
    HistoryStack_Free(&editor->undoStack);
    HistoryStack_Free(&editor->redoStack);
    free(editor->anchorSelection);
    editor->anchorSelection = NULL;
    editor->anchorSelectionCapacity = 0;
    editor->anchorSelectionCount = 0;
    free(editor->dragSnapshots);
    editor->dragSnapshots = NULL;
    editor->dragSnapshotCapacity = 0;
    editor->dragSnapshotCount = 0;
}

void Editor_SetShiftHeld(EditorState* editor, bool held) {
    editor->shiftHeld = held;
}

void Editor_HistoryCapture(EditorState* editor, const Layout* layout) {
    if (!editor || !layout) return;
    char* snapshot = Layout_SaveToString(layout);
    if (!snapshot) return;

    HistoryStack_Push(&editor->undoStack, snapshot);
    HistoryStack_Reset(&editor->redoStack);
}

static void Editor_ResetSelection(EditorState* editor) {
    Editor_ClearAnchorSelection(editor);
    editor->selectedWallIndex = -1;
    editor->selectedHandleAnchor = -1;
    editor->selectedHandleComponent = -1;
    editor->selectedObject3DId = 0u;
    editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    editor->hoveredGizmoAxis = -1;
    editor->hoveredObject3DGizmoAxis = -1;
    editor->activeObject3DGizmoAxis = -1;
    editor->hoveredObject3DId = 0u;
    editor->hoveredObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    editor->hoveredObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    editor->isDraggingAnchor = false;
    editor->isResizingObject3D = false;
    editor->isRotatingObject3D = false;
    editor->isPreciseDrag = false;
    editor->selectionBoxActive = false;
    editor->selectionBoxAdditive = false;
    editor->mode = TOOL_IDLE;
    Editor_ResetGizmoDrag(editor);
}

void Editor_SelectAnchorsInBox(EditorState* editor, const Layout* layout, Vec2 min, Vec2 max, bool additive) {
    if (!editor || !layout) return;
    editor->selectedObject3DId = 0u;
    editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    if (!additive) {
        AnchorSelection_Clear(editor);
    }
    GlobalState* state = Global_Get();
    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);

    for (size_t i = 0; i < layout->anchorCount; ++i) {
        const Anchor* anchor = &layout->anchors[i];
        if (anchor->isDeleted) continue;
        Vec2 pos = SpaceAdapter_ProjectToView(anchor->pos, &viewCtx);
        if (pos.x >= min.x && pos.x <= max.x &&
            pos.y >= min.y && pos.y <= max.y) {
            AnchorSelection_Add(editor, (int)i);
        }
    }

    if (editor->anchorSelectionCount > 0) {
        editor->selectedAnchorIndex = editor->anchorSelection[editor->anchorSelectionCount - 1];
    } else {
        editor->selectedAnchorIndex = -1;
    }
}

bool Editor_Undo(EditorState* editor, Layout* layout) {
    if (!editor || !layout) return false;
    char* snapshot = HistoryStack_Pop(&editor->undoStack);
    if (!snapshot) return false;

    char* current = Layout_SaveToString(layout);
    if (current) {
        HistoryStack_Push(&editor->redoStack, current);
    }

    bool ok = Layout_LoadFromString(layout, snapshot);
    free(snapshot);

    if (ok) {
        Editor_ResetSelection(editor);
        Global_FlagHitboxesDirty();
    }
    return ok;
}

bool Editor_Redo(EditorState* editor, Layout* layout) {
    if (!editor || !layout) return false;
    char* snapshot = HistoryStack_Pop(&editor->redoStack);
    if (!snapshot) return false;

    char* current = Layout_SaveToString(layout);
    if (current) {
        HistoryStack_Push(&editor->undoStack, current);
    }

    bool ok = Layout_LoadFromString(layout, snapshot);
    free(snapshot);

    if (ok) {
        Editor_ResetSelection(editor);
        Global_FlagHitboxesDirty();
    }
    return ok;
}

size_t Editor_UndoCount(const EditorState* editor) {
    return editor ? editor->undoStack.count : 0;
}

size_t Editor_RedoCount(const EditorState* editor) {
    return editor ? editor->redoStack.count : 0;
}

void Editor_ClearHistory(EditorState* editor) {
    if (!editor) return;
    HistoryStack_Reset(&editor->undoStack);
    HistoryStack_Reset(&editor->redoStack);
}

void Editor_ClearAnchorSelection(EditorState* editor) {
    if (!editor) return;
    AnchorSelection_Clear(editor);
    editor->selectedAnchorIndex = -1;
    editor->dragSnapshotCount = 0;
    editor->hoveredGizmoAxis = -1;
    editor->hoveredObject3DGizmoAxis = -1;
    editor->selectedObject3DId = 0u;
    editor->hoveredObject3DId = 0u;
    editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    editor->hoveredObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    editor->hoveredObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    editor->activeObject3DGizmoAxis = -1;
    editor->isResizingObject3D = false;
    editor->isRotatingObject3D = false;
    Editor_ResetGizmoDrag(editor);
}

void Editor_SelectAnchor(EditorState* editor, int anchorIndex, bool additive) {
    if (!editor) return;
    if (!additive) {
        AnchorSelection_Clear(editor);
    }
    if (anchorIndex < 0) {
        editor->selectedAnchorIndex = -1;
        editor->selectedObject3DId = 0u;
        editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
        editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
        return;
    }
    AnchorSelection_Add(editor, anchorIndex);
    editor->selectedAnchorIndex = anchorIndex;
    editor->selectedObject3DId = 0u;
    editor->selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    editor->selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
}

bool Editor_IsAnchorSelected(const EditorState* editor, int anchorIndex) {
    if (!editor || anchorIndex < 0) return false;
    for (int i = 0; i < editor->anchorSelectionCount; ++i) {
        if (editor->anchorSelection[i] == anchorIndex) return true;
    }
    return false;
}

int Editor_SelectedAnchorCount(const EditorState* editor) {
    return editor ? editor->anchorSelectionCount : 0;
}

void Editor_BeginAnchorDrag(EditorState* editor, const Layout* layout) {
    if (!editor || !layout) return;
    if (editor->selectedAnchorIndex >= 0 && editor->anchorSelectionCount == 0) {
        Editor_SelectAnchor(editor, editor->selectedAnchorIndex, false);
    }
    int count = editor->anchorSelectionCount;
    if (count == 0) return;
    DragSnapshot_EnsureCapacity(editor, count);
    if (!editor->dragSnapshots) {
        editor->dragSnapshotCount = 0;
        return;
    }

    int primary = editor->selectedAnchorIndex;
    int write = 0;
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < editor->anchorSelectionCount; ++i) {
            int idx = editor->anchorSelection[i];
            bool isPrimary = (idx == primary);
            if ((pass == 0 && isPrimary) || (pass == 1 && !isPrimary)) {
                if (idx >= 0 && (size_t)idx < layout->anchorCount) {
                    editor->dragSnapshots[write].anchorIndex = idx;
                    editor->dragSnapshots[write].startPos = layout->anchors[idx].pos;
                    write++;
                }
            }
        }
    }
    editor->dragSnapshotCount = write;
}

void Editor_UpdateAnchorDrag(EditorState* editor, Layout* layout, Vec3 primaryNewPos) {
    if (!editor || !layout) return;
    if (editor->dragSnapshotCount == 0) return;

    int primaryIndex = editor->dragSnapshots[0].anchorIndex;
    Vec3 primaryStart = editor->dragSnapshots[0].startPos;
    Vec3 delta = Vec3_Sub(primaryNewPos, primaryStart);

    for (int i = 0; i < editor->dragSnapshotCount; ++i) {
        int idx = editor->dragSnapshots[i].anchorIndex;
        if (idx < 0 || (size_t)idx >= layout->anchorCount) continue;
        Vec3 base = editor->dragSnapshots[i].startPos;
        Vec3 newPos = (idx == primaryIndex) ? primaryNewPos : Vec3_Add(base, delta);
        layout->anchors[idx].pos = newPos;
    }
    Global_FlagLayoutChanged();
}

void Editor_EndAnchorDrag(EditorState* editor) {
    if (!editor) return;
    editor->dragSnapshotCount = 0;
}

void Editor_ClickAt(EditorState* editor, Vec3 worldPos) {
    GlobalState* state = Global_Get();
    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    Layout* layout = &state->layout;

    if (editor->mode == TOOL_IDLE) {
        editor->anchor = worldPos;
        editor->mode = TOOL_PLACING_WALL;
    } else if (editor->mode == TOOL_PLACING_WALL) {
        Vec3 from = editor->anchor;
        Vec3 to = worldPos;
        ViewPlaneAxis planeAxis = SpaceAdapter_ActivePlaneAxis(&viewCtx);
        float planeOffset = SpaceAdapter_ActivePlaneOffset(&viewCtx);

        if (editor->shiftHeld) {
            Vec2 from2 = Vec3_ProjectToPlane(from, planeAxis);
            Vec2 to2 = Vec3_ProjectToPlane(to, planeAxis);
            float dx = fabsf(to2.x - from2.x);
            float dy = fabsf(to2.y - from2.y);
            if (dx > dy) to2.y = from2.y;
            else         to2.x = from2.x;
            to = Vec3_FromPlaneCoords(to2, planeAxis, planeOffset);
        }

        Editor_HistoryCapture(editor, layout);
        Layout_AddWall3(layout, from, to);
        editor->mode = TOOL_IDLE;
    }
}
