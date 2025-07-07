// src/Math/math_util.h
#pragma once
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

