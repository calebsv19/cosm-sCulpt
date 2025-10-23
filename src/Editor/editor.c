// src/Editor/editor.c
#include "Editor/editor.h"
#include "Core/global_state.h"
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

void Editor_Init(EditorState* editor) {
    editor->mode = TOOL_IDLE;
    editor->shiftHeld = false;
    editor->anchor = (Vec2){0, 0};
    editor->selectedWallIndex = -1;
    editor->selectedAnchorIndex = -1;
    editor->deleteMode = DELETE_MODE_SAFE;
    HistoryStack_Init(&editor->undoStack);
    HistoryStack_Init(&editor->redoStack);
}

void Editor_Free(EditorState* editor) {
    HistoryStack_Free(&editor->undoStack);
    HistoryStack_Free(&editor->redoStack);
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
    editor->selectedAnchorIndex = -1;
    editor->selectedWallIndex = -1;
    editor->mode = TOOL_IDLE;
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

void Editor_ClickAt(EditorState* editor, Vec2 worldPos) {
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    if (editor->mode == TOOL_IDLE) {
        editor->anchor = worldPos;
        editor->mode = TOOL_PLACING_WALL;
    } else if (editor->mode == TOOL_PLACING_WALL) {
        Vec2 from = editor->anchor;
        Vec2 to = worldPos;

        if (editor->shiftHeld) {
            float dx = fabsf(to.x - from.x);
            float dy = fabsf(to.y - from.y);
            if (dx > dy) to.y = from.y;
            else         to.x = from.x;
        }

        Editor_HistoryCapture(editor, layout);
        Layout_AddWall(layout, from, to);
        editor->mode = TOOL_IDLE;
    }
}
