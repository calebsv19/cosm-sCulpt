// src/Tools/shape_draw_sdl.c

#include "Tools/shape_draw_sdl.h"
#include "Tools/shape_flatten.h"

#include <float.h>
#include <math.h>

static void ShapeTool_WorldToScreen(Vec2 pt,
                                    float centerX, float centerY,
                                    float scale,
                                    int screenW, int screenH,
                                    int* outX, int* outY) {
    float sx = (pt.x - centerX) * scale + (float)screenW * 0.5f;
    float sy = (pt.y - centerY) * scale + (float)screenH * 0.5f;
    if (outX) *outX = (int)lroundf(sx);
    if (outY) *outY = (int)lroundf(sy);
}

bool Shape_DrawToSDL(SDL_Renderer* renderer,
                     const Shape* shape,
                     int screenW, int screenH,
                     float maxError) {
    if (!renderer || !shape || screenW <= 0 || screenH <= 0) return false;

    PolylineSet set;
    if (!Shape_FlattenToPolylines(shape, maxError, &set)) {
        return false;
    }

    bool hasPoints = false;
    float minX = FLT_MAX;
    float maxX = -FLT_MAX;
    float minY = FLT_MAX;
    float maxY = -FLT_MAX;

    for (size_t i = 0; i < set.count; ++i) {
        Polyline* line = &set.lines[i];
        for (size_t j = 0; j < line->count; ++j) {
            Vec2 p = line->points[j];
            if (p.x < minX) minX = p.x;
            if (p.x > maxX) maxX = p.x;
            if (p.y < minY) minY = p.y;
            if (p.y > maxY) maxY = p.y;
            hasPoints = true;
        }
    }

    if (!hasPoints) {
        PolylineSet_Free(&set);
        return true;
    }

    float width = maxX - minX;
    float height = maxY - minY;
    float margin = 0.1f;
    float availW = screenW * (1.0f - margin * 2.0f);
    float availH = screenH * (1.0f - margin * 2.0f);
    if (availW <= 0.0f) availW = (float)screenW;
    if (availH <= 0.0f) availH = (float)screenH;

    float scaleX = (width > 1e-6f) ? (availW / width) : availW;
    float scaleY = (height > 1e-6f) ? (availH / height) : availH;
    float scale = fminf(scaleX, scaleY);
    if (scale <= 0.0f) scale = 1.0f;

    float centerX = 0.5f * (minX + maxX);
    float centerY = 0.5f * (minY + maxY);

    SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255);

    for (size_t i = 0; i < set.count; ++i) {
        Polyline* line = &set.lines[i];
        if (line->count < 2) continue;

        Vec2 prev = line->points[0];
        int prevX = 0;
        int prevY = 0;
        ShapeTool_WorldToScreen(prev, centerX, centerY, scale, screenW, screenH, &prevX, &prevY);

        for (size_t j = 1; j < line->count; ++j) {
            Vec2 curr = line->points[j];
            int currX = 0;
            int currY = 0;
            ShapeTool_WorldToScreen(curr, centerX, centerY, scale, screenW, screenH, &currX, &currY);
            SDL_RenderDrawLine(renderer, prevX, prevY, currX, currY);
            prevX = currX;
            prevY = currY;
        }

        if (line->closed && line->count > 1) {
            Vec2 first = line->points[0];
            int firstX = 0;
            int firstY = 0;
            ShapeTool_WorldToScreen(first, centerX, centerY, scale, screenW, screenH, &firstX, &firstY);
            SDL_RenderDrawLine(renderer, prevX, prevY, firstX, firstY);
        }
    }

    PolylineSet_Free(&set);
    return true;
}
