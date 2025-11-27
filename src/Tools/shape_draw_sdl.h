// src/Tools/shape_draw_sdl.h
#pragma once

#include <SDL2/SDL.h>
#include "Tools/shape_asset.h"

typedef struct {
    float scale;
    Vec2  offset;
} ShapeViewTransform;

bool Shape_DrawToSDL(SDL_Renderer* renderer,
                     const Shape* shape,
                     int screenW, int screenH,
                     float maxError);
