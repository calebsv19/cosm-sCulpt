// src/Render/render_grid.c
#include "Layout/Grid/render_grid.h"
#include <math.h>


static void DrawGridLayer(const Grid* g, SDL_Renderer* r, int width, int height, int mod, SDL_Color color) {
    float pixelsPerUnit = g->gridSize * g->scale;

    // Compute world-space bounds of screen
    float worldLeft   = g->offsetX;
    float worldRight  = g->offsetX + (float)width / pixelsPerUnit;
    float worldTop    = g->offsetY;
    float worldBottom = g->offsetY + (float)height / pixelsPerUnit;

    // Find first line index for this mod
    int firstX = (int)floorf(worldLeft);
    int firstY = (int)floorf(worldTop);

    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);

    // Vertical lines
    for (int i = firstX; i <= (int)ceilf(worldRight); ++i) {
        if (i % mod != 0) continue;

        float x = (i - g->offsetX) * pixelsPerUnit;
        SDL_RenderDrawLine(r, (int)x, 0, (int)x, height);
    }

    // Horizontal lines
    for (int j = firstY; j <= (int)ceilf(worldBottom); ++j) {
        if (j % mod != 0) continue;

        float y = (j - g->offsetY) * pixelsPerUnit;
        SDL_RenderDrawLine(r, 0, (int)y, width, (int)y);
    }
}


void Render_Grid(const Grid* g, SDL_Renderer* renderer, int width, int height, int drawFlags) {
    float step = g->gridSize * g->scale;
    if (step < 10.0f) return;

    // Pass 1: Unit lines (1x1, not divisible by 5 or 10)
        if (drawFlags & GRID_DRAW_UNITS) {
        SDL_Color color = {60, 60, 60, 50};
        DrawGridLayer(g, renderer, width, height, 1, color);
    }


    // Pass 2: 5-unit lines (divisible by 5, not 10)
    if (drawFlags & GRID_DRAW_FIVES) {
        SDL_Color color = {130, 130, 130, 180};
        DrawGridLayer(g, renderer, width, height, 5, color);
    }

    // Pass 3: 10-unit lines (divisible by 10)
    if (drawFlags & GRID_DRAW_TENS) {
        SDL_Color color = {255, 255, 255, 255};
        DrawGridLayer(g, renderer, width, height, 10, color);
    }

    // Origin "+"
    int ox = (int)((0.0f - g->offsetX) * g->gridSize * g->scale);
    int oy = (int)((0.0f - g->offsetY) * g->gridSize * g->scale);

    SDL_SetRenderDrawColor(renderer, 255, 100, 100, 255);
    SDL_RenderDrawLine(renderer, ox - 3, oy, ox + 3, oy);
    SDL_RenderDrawLine(renderer, ox, oy - 3, ox, oy + 3);
}


