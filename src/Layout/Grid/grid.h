// src/grid.h
#ifndef GRID_H
#define GRID_H

#include <SDL2/SDL.h>

// Represents the grid state: cell size, pan offset, and zoom scale
typedef struct {
    float gridSize;    // world units per grid cell
    float offsetX;     // world offset
    float offsetY;
    float scale;       // screen pixels per world unit
} Grid;

enum {
    GRID_DEFAULT_MIN_SCALE_INT = 10,
    GRID_DEFAULT_MAX_SCALE_INT = 100
};

#define GRID_DEFAULT_MIN_SCALE ((float)GRID_DEFAULT_MIN_SCALE_INT)
#define GRID_DEFAULT_MAX_SCALE ((float)GRID_DEFAULT_MAX_SCALE_INT)

// Initialize the grid with a given cell size (in world units)
void Grid_init(Grid* g, float gridSize, int screenW, int screenH);

// Pan the view by dx, dy (in screen pixels)
void Grid_pan(Grid *g, float dx_pixels, float dy_pixels);

// Zoom by factor (e.g. 1.1 to zoom in), centered on screen point (cx, cy)
void Grid_zoom(Grid *g, float zoomFactor, float cx, float cy);

// Zoom with explicit scale limits while keeping the anchor point fixed.
void Grid_zoom_clamped(Grid* g,
                       float zoomFactor,
                       float cx,
                       float cy,
                       float minScale,
                       float maxScale);

#endif // GRID_H
