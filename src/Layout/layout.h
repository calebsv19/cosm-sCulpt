// src/Layout/layout.h
#pragma once

#include <stddef.h>
#include "Math/math_util.h"

typedef struct {
    Vec2 from;
    Vec2 to;
} Wall;

typedef struct {
    float gridSize;
    Wall* walls;
    size_t wallCount;
} Layout;

void Layout_Init(Layout* layout, float gridSize);
void Layout_AddWall(Layout* layout, Vec2 from, Vec2 to);
void Layout_Free(Layout* layout);

