// src/Editor/editor.c
#include "Editor/editor.h"
#include "Core/global_state.h"
#include "Math/math_util.h"
#include "Layout/layout.h"
#include <SDL2/SDL.h>
#include <math.h>

void Editor_Init(EditorState* editor) {
    editor->mode = TOOL_IDLE;
    editor->shiftHeld = false;
    editor->anchor = (Vec2){0, 0};
}

void Editor_SetShiftHeld(EditorState* editor, bool held) {
    editor->shiftHeld = held;
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

        Layout_AddWall(layout, from, to);
        editor->mode = TOOL_IDLE;
    }
}

void Editor_Render(EditorState* editor, AppContext* ctx) {
    if (editor->mode != TOOL_PLACING_WALL)
        return;

    GlobalState* state = Global_Get();
    Grid* grid = &state->grid;
    float scale = grid->scale;
    float gridSize = grid->gridSize;

    int mx, my;
    SDL_GetMouseState(&mx, &my);
    Vec2 to = {
        .x = mx / (gridSize * scale) + grid->offsetX,
        .y = my / (gridSize * scale) + grid->offsetY
    };
    to = Vec2_Snap(to, gridSize);

    Vec2 from = editor->anchor;
    if (editor->shiftHeld) {
        float dx = fabsf(to.x - from.x);
        float dy = fabsf(to.y - from.y);
        if (dx > dy) to.y = from.y;
        else         to.x = from.x;
    }

    SDL_SetRenderDrawColor(ctx->renderer, 255, 100, 100, 255);
    SDL_RenderDrawLine(ctx->renderer,
        (int)((from.x - grid->offsetX) * gridSize * scale),
        (int)((from.y - grid->offsetY) * gridSize * scale),
        (int)((to.x   - grid->offsetX) * gridSize * scale),
        (int)((to.y   - grid->offsetY) * gridSize * scale));
}

