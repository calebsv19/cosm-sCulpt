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

static inline float DegToRad(float deg) {
    return deg * (float)M_PI / 180.0f;
}

static inline float RadToDeg(float rad) {
    return rad * 180.0f / (float)M_PI;
}

static inline Vec2 Vec2_FromPolar(float length, float angleDeg) {
    float rad = DegToRad(angleDeg);
    return (Vec2){
        .x = cosf(rad) * length,
        .y = sinf(rad) * length
    };
}

static inline Vec2 Vec2_Add(Vec2 a, Vec2 b) {
    return (Vec2){ a.x + b.x, a.y + b.y };
}

static inline Vec2 Vec2_Sub(Vec2 a, Vec2 b) {
    return (Vec2){ a.x - b.x, a.y - b.y };
}

static inline Vec2 Vec2_HandleAbsolute(Vec2 anchorPos, float length, float angleDeg) {
    return Vec2_Add(anchorPos, Vec2_FromPolar(length, angleDeg));
}

static inline float Vec2_AngleDeg(Vec2 anchorPos, Vec2 targetPos) {
    return RadToDeg(atan2f(targetPos.y - anchorPos.y, targetPos.x - anchorPos.x));
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

static inline float Angle_NormalizeDeg(float angle) {
    float result = fmodf(angle, 360.0f);
    if (result < 0.0f) result += 360.0f;
    return result;
}
