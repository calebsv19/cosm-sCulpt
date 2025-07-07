// src/Math/math_util.h
#pragma once

#include "Layout/Grid/grid.h"
#include <math.h>


typedef struct { float x, y; } Vec2;

static inline float Vec2_Distance(Vec2 a, Vec2 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return sqrtf(dx*dx + dy*dy);
}

static inline Vec2 Vec2_Snap(Vec2 pt, float gridSize) {
    pt.x = roundf(pt.x / gridSize) * gridSize;
    pt.y = roundf(pt.y / gridSize) * gridSize;
    return pt;
}


static inline Vec2 WorldToScreen(Vec2 world, const Grid* grid) {
    return (Vec2){
        .x = (world.x - grid->offsetX) * grid->gridSize * grid->scale,
        .y = (world.y - grid->offsetY) * grid->gridSize * grid->scale
    };
}

static inline Vec2 ScreenToWorld(int screenX, int screenY, const Grid* grid) {
    return (Vec2){
        .x = (screenX / (grid->gridSize * grid->scale)) + grid->offsetX,
        .y = (screenY / (grid->gridSize * grid->scale)) + grid->offsetY
    };
}

static inline Vec2 ScreenToSnappedWorld(int screenX, int screenY, const Grid* grid) {
    Vec2 world = ScreenToWorld(screenX, screenY, grid);
    return Vec2_Snap(world, grid->gridSize);
}

