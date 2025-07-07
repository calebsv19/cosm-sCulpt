// src/Render/render_grid.h
#pragma once
#include <SDL2/SDL.h>
#include "Layout/Grid/grid.h"

#define GRID_DRAW_UNITS   1
#define GRID_DRAW_FIVES   2
#define GRID_DRAW_TENS    4

// Draws a precision-aligned grid using floating-point math to avoid drift
void Render_Grid(const Grid* g, SDL_Renderer* renderer, int width, int height, int drawFlags);
