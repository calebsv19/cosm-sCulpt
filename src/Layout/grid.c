// src/grid.c
#include "grid.h"

void Grid_init(Grid *g, float gridSize) {
    g->gridSize = gridSize;
    g->offsetX = 0.0f;
    g->offsetY = 0.0f;
    g->scale   = 1.0f;
}

void Grid_pan(Grid *g, float dx_pixels, float dy_pixels) {
    g->offsetX += dx_pixels / g->scale;
    g->offsetY += dy_pixels / g->scale;
}

void Grid_draw(const Grid *g, SDL_Renderer *renderer, int width, int height) {
    SDL_SetRenderDrawColor(renderer, 68, 66, 60, 255);

    float step = g->gridSize * g->scale;
    if (step < 10.0f) return;

    float offsetXPx = g->offsetX * step;
    float offsetYPx = g->offsetY * step;

    int startX = (int)fmodf(-offsetXPx, step);
    if (startX < 0) startX += (int)step;
    int startY = (int)fmodf(-offsetYPx, step);
    if (startY < 0) startY += (int)step;

    for (int x = startX; x < width; x += (int)step)
        SDL_RenderDrawLine(renderer, x, 0, x, height);

    for (int y = startY; y < height; y += (int)step)
        SDL_RenderDrawLine(renderer, 0, y, width, y);
}

void Grid_zoom(Grid *g, float zoomFactor, float cx_px, float cy_px) {
    float oldScale = g->scale;
    g->scale *= zoomFactor;

    if (g->scale < 10.0f) g->scale = 10.0f;
    if (g->scale > 100.0f) g->scale = 100.0f;

    float cx_world = cx_px / (oldScale * g->gridSize);
    float cy_world = cy_px / (oldScale * g->gridSize);

    g->offsetX = cx_world + (g->offsetX - cx_world) * (oldScale / g->scale);
    g->offsetY = cy_world + (g->offsetY - cy_world) * (oldScale / g->scale);

}

