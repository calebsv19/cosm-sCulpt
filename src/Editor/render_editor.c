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
    for (int w = 0; w < radius * 2; w++) {
        for (int h = 0; h < radius * 2; h++) {
            int dx = radius - w;
            int dy = radius - h;
            if (dx * dx + dy * dy <= radius * radius)
                SDL_RenderDrawPoint(ctx->renderer, cx + dx, cy + dy);
        }
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

