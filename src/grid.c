// src/grid.c
#include "grid.h"

void Grid_init(Grid *g, float gridSize) {
    g->gridSize = gridSize;
    g->offsetX = 0.0f;
    g->offsetY = 0.0f;
    g->scale   = 1.0f;
}

void Grid_pan(Grid *g, float dx, float dy) {
    g->offsetX += dx;
    g->offsetY += dy;
}

void Grid_zoom(Grid *g, float zoomFactor, float cx, float cy) {
    // Keep the point (cx,cy) fixed under zoom
    float oldScale = g->scale;
    g->scale *= zoomFactor;
    g->offsetX = cx + (g->offsetX - cx) * (g->scale / oldScale);
    g->offsetY = cy + (g->offsetY - cy) * (g->scale / oldScale);
}

void Grid_draw(const Grid *g, SDL_Renderer *renderer, int width, int height) {
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    // pixel spacing between lines
    float step = g->gridSize * g->scale;
    if (step < 10.0f) return; // avoid overcrowding

    // find starting offset so lines pan correctly
    int startX = (int)fmodf(-g->offsetX, step);
    int startY = (int)fmodf(-g->offsetY, step);

    for (int x = startX; x < width; x += (int)step) {
        SDL_RenderDrawLine(renderer, x, 0, x, height);
    }
    for (int y = startY; y < height; y += (int)step) {
        SDL_RenderDrawLine(renderer, 0, y, width, y);
    }
}

