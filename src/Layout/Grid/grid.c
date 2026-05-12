#include "grid.h"

#include <math.h>

void Grid_init(Grid* g, float gridSize, int screenW, int screenH) {
    g->gridSize = gridSize;
    g->scale    = 32.0f;  // Moved here

    float pixelsPerUnit = gridSize * g->scale;

    float centerWorldX = (float)screenW / 2.0f / pixelsPerUnit;
    float centerWorldY = (float)screenH / 2.0f / pixelsPerUnit;

    g->offsetX = -centerWorldX;
    g->offsetY = -centerWorldY;
}


void Grid_pan(Grid* g, float dx_pixels, float dy_pixels) {
    float pixelsPerUnit = g->gridSize * g->scale;
    g->offsetX += dx_pixels / pixelsPerUnit;
    g->offsetY += dy_pixels / pixelsPerUnit;
}


void Grid_zoom_clamped(Grid* g,
                       float zoomFactor,
                       float cx_px,
                       float cy_px,
                       float minScale,
                       float maxScale) {
    if (!g) return;
    if (!isfinite(zoomFactor) || zoomFactor <= 0.0f) return;
    if (!isfinite(minScale) || minScale <= 0.0f) {
        minScale = GRID_DEFAULT_MIN_SCALE;
    }
    if (!isfinite(maxScale) || maxScale < minScale) {
        maxScale = GRID_DEFAULT_MAX_SCALE;
    }

    float oldScale = g->scale;
    float newScale = g->scale * zoomFactor;

    if (newScale < minScale) newScale = minScale;
    if (newScale > maxScale) newScale = maxScale;

    float pixelsPerUnitOld = oldScale * g->gridSize;
    float pixelsPerUnitNew = newScale * g->gridSize;
    if (pixelsPerUnitOld <= 0.0f || pixelsPerUnitNew <= 0.0f) return;

    // Convert center point to world coords before zoom
    float cx_world = cx_px / pixelsPerUnitOld + g->offsetX;
    float cy_world = cy_px / pixelsPerUnitOld + g->offsetY;

    // Update scale
    g->scale = newScale;

    // After zoom, compute new offset so (cx_world) stays fixed on screen
    g->offsetX = cx_world - cx_px / pixelsPerUnitNew;
    g->offsetY = cy_world - cy_px / pixelsPerUnitNew;
}

void Grid_zoom(Grid* g, float zoomFactor, float cx_px, float cy_px) {
    Grid_zoom_clamped(g,
                      zoomFactor,
                      cx_px,
                      cy_px,
                      GRID_DEFAULT_MIN_SCALE,
                      GRID_DEFAULT_MAX_SCALE);
}
