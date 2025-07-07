// src/grid.h
#ifndef GRID_H
#define GRID_H

#include <SDL2/SDL.h>

// Represents the grid state: cell size, pan offset, and zoom scale
typedef struct {
    float gridSize;    // world units per grid cell
    float offsetX;     // pan offset in screen pixels
    float offsetY;
    float scale;       // screen pixels per world unit
} Grid;

// Initialize the grid with a given cell size (in world units)
void Grid_init(Grid *g, float gridSize);

// Pan the view by dx, dy (in screen pixels)
void Grid_pan(Grid *g, float dx, float dy);

// Zoom by factor (e.g. 1.1 to zoom in), centered on screen point (cx, cy)
void Grid_zoom(Grid *g, float zoomFactor, float cx, float cy);

// Draw the grid into the current renderer; width/height are window dimensions
void Grid_draw(const Grid *g, SDL_Renderer *renderer, int width, int height);

#endif // GRID_H

