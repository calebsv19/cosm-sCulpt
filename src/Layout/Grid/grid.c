// src/grid.c
#include "grid.h"

void Grid_init(Grid* g, float gridSize, int screenW, int screenH) {
    g->gridSize = gridSize;
    g->scale    = 1.0f;

    float pixelsPerUnit = gridSize * g->scale;

    // Calculate center of screen in world units
    float centerWorldX = (float)screenW / 2.0f / pixelsPerUnit;
    float centerWorldY = (float)screenH / 2.0f / pixelsPerUnit;

    // Snap origin to nearest 10x10 world unit cross
    centerWorldX = roundf(centerWorldX / 10.0f) * 10.0f;
    centerWorldY = roundf(centerWorldY / 10.0f) * 10.0f;

    g->offsetX = centerWorldX;
    g->offsetY = centerWorldY;
}


void Grid_pan(Grid* g, float dx_pixels, float dy_pixels) {
    float pixelsPerUnit = g->gridSize * g->scale;
    g->offsetX += dx_pixels / pixelsPerUnit;
    g->offsetY += dy_pixels / pixelsPerUnit;
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

