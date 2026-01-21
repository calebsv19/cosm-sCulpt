// src/Render/render_editor.c
#include "Editor/render_editor.h"
#include "Core/global_state.h"
#include "Layout/Grid/grid.h"
#include "Math/math_util.h"
#include <SDL2/SDL.h>
#include <math.h>

// ─────────────────────────────────────────────
// High-level combined renderer
void Render_EditorOverlay(EditorState* editor, AppContext* ctx) {
    Render_Editor_Anchor(editor, ctx);
    Render_Editor_GhostWall(editor, ctx);
    // Future:
    // Render_Editor_SelectedWall(editor, ctx);
}

// ─────────────────────────────────────────────
// Draw the anchor point circle
void Render_Editor_Anchor(EditorState* editor, AppContext* ctx) {
    if (editor->mode != TOOL_PLACING_WALL) return;

    GlobalState* state = Global_Get();
    Grid* grid = &state->grid;

    float gridSize = grid->gridSize;
    float scale = grid->scale;

    Vec2 from = editor->anchor;

    int cx = (int)((from.x - grid->offsetX) * gridSize * scale);
    int cy = (int)((from.y - grid->offsetY) * gridSize * scale);
    int radius = (int)(gridSize * scale * 0.15f);

    SDL_SetRenderDrawColor(ctx->renderer, 255, 255, 100, 255);  // yellow
    int r2 = radius * radius;
    for (int dy = -radius; dy <= radius; ++dy) {
        int dx = (int)sqrtf((float)(r2 - dy * dy));
        SDL_RenderDrawLine(ctx->renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

// ─────────────────────────────────────────────
// Draw the ghost wall (anchor to mouse)
void Render_Editor_GhostWall(EditorState* editor, AppContext* ctx) {
    if (editor->mode != TOOL_PLACING_WALL) return;

    GlobalState* state = Global_Get();
    Grid* grid = &state->grid;

    float gridSize = grid->gridSize;
    float scale = grid->scale;

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

    SDL_SetRenderDrawColor(ctx->renderer, 255, 100, 100, 255);  // red
    SDL_RenderDrawLine(ctx->renderer,
        (int)((from.x - grid->offsetX) * gridSize * scale),
        (int)((from.y - grid->offsetY) * gridSize * scale),
        (int)((to.x   - grid->offsetX) * gridSize * scale),
        (int)((to.y   - grid->offsetY) * gridSize * scale));
}

void Render_Editor_SelectionBox(EditorState* editor, AppContext* ctx) {
    if (!editor->selectionBoxActive) return;

    GlobalState* state = Global_Get();
    const Grid* grid = &state->grid;

    Vec2 start = WorldToScreen(editor->selectionBoxStart, grid);
    Vec2 end = WorldToScreen(editor->selectionBoxEnd, grid);

    float minX = fminf(start.x, end.x);
    float minY = fminf(start.y, end.y);
    float maxX = fmaxf(start.x, end.x);
    float maxY = fmaxf(start.y, end.y);

    SDL_Rect rect = {
        .x = (int)minX,
        .y = (int)minY,
        .w = (int)(maxX - minX),
        .h = (int)(maxY - minY)
    };

    if (rect.w == 0 || rect.h == 0) return;

    SDL_SetRenderDrawColor(ctx->renderer, 80, 150, 255, 60);
    SDL_RenderFillRect(ctx->renderer, &rect);
    SDL_SetRenderDrawColor(ctx->renderer, 80, 150, 255, 200);
    SDL_RenderDrawRect(ctx->renderer, &rect);
}
