// src/Tools/shape_flatten.c

#include "Tools/shape_flatten.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    Vec2*  points;
    size_t count;
    size_t capacity;
} PolylineBuilder;

static bool PolylineBuilder_Append(PolylineBuilder* builder, Vec2 pt) {
    if (!builder) return false;
    if (builder->count > 0) {
        Vec2 prev = builder->points[builder->count - 1];
        if (fabsf(prev.x - pt.x) < 1e-6f && fabsf(prev.y - pt.y) < 1e-6f) {
            return true;
        }
    }

    if (builder->count >= builder->capacity) {
        size_t newCap = builder->capacity ? builder->capacity * 2 : 32;
        Vec2* tmp = (Vec2*)realloc(builder->points, newCap * sizeof(Vec2));
        if (!tmp) return false;
        builder->points = tmp;
        builder->capacity = newCap;
    }
    builder->points[builder->count++] = pt;
    return true;
}

static float Vec2_Length(Vec2 v) {
    return sqrtf(v.x * v.x + v.y * v.y);
}

static bool Vec2_NearlyEqual(Vec2 a, Vec2 b, float eps) {
    return fabsf(a.x - b.x) <= eps && fabsf(a.y - b.y) <= eps;
}

static Vec2 CubicEval(Vec2 p0, Vec2 c1, Vec2 c2, Vec2 p1, float t) {
    float u = 1.0f - t;
    float uu = u * u;
    float uuu = uu * u;
    float tt = t * t;
    float ttt = tt * t;

    Vec2 r;
    r.x = uuu * p0.x +
          3.0f * uu * t * c1.x +
          3.0f * u * tt * c2.x +
          ttt * p1.x;
    r.y = uuu * p0.y +
          3.0f * uu * t * c1.y +
          3.0f * u * tt * c2.y +
          ttt * p1.y;
    return r;
}

static float CubicEstimateLength(Vec2 p0, Vec2 c1, Vec2 c2, Vec2 p1) {
    const int samples = 8;
    Vec2 prev = p0;
    float length = 0.0f;
    for (int i = 1; i <= samples; ++i) {
        float t = (float)i / (float)samples;
        Vec2 cur = CubicEval(p0, c1, c2, p1, t);
        Vec2 delta = {
            cur.x - prev.x,
            cur.y - prev.y
        };
        length += Vec2_Length(delta);
        prev = cur;
    }
    return length;
}

static void PolylineBuilder_Free(PolylineBuilder* builder) {
    if (!builder) return;
    free(builder->points);
    builder->points = NULL;
    builder->count = 0;
    builder->capacity = 0;
}

static bool FlattenPath(const ShapePath* path, float maxError, Polyline* out) {
    if (!path || !out) return false;

    PolylineBuilder builder = {0};

    for (size_t si = 0; si < path->segmentCount; ++si) {
        const ShapeSegment* seg = &path->segments[si];
        if (!seg) continue;

        switch (seg->type) {
            case SHAPE_SEGMENT_LINE: {
                if (builder.count == 0 && !PolylineBuilder_Append(&builder, seg->p0)) {
                    PolylineBuilder_Free(&builder);
                    return false;
                }
                if (!PolylineBuilder_Append(&builder, seg->p1)) {
                    PolylineBuilder_Free(&builder);
                    return false;
                }
                break;
            }
            case SHAPE_SEGMENT_CUBIC_BEZIER: {
                if (builder.count == 0 && !PolylineBuilder_Append(&builder, seg->p0)) {
                    PolylineBuilder_Free(&builder);
                    return false;
                }

                float approxLen = CubicEstimateLength(seg->p0, seg->c1, seg->c2, seg->p1);
                float targetSpacing = fmaxf(maxError, 0.01f);
                int steps = (int)ceilf(approxLen / targetSpacing);
                const int minSteps = 4;
                const int maxSteps = 96;
                if (steps < minSteps) steps = minSteps;
                if (steps > maxSteps) steps = maxSteps;

                for (int step = 1; step <= steps; ++step) {
                    float t = (float)step / (float)steps;
                    Vec2 point = CubicEval(seg->p0, seg->c1, seg->c2, seg->p1, t);
                    if (!PolylineBuilder_Append(&builder, point)) {
                        PolylineBuilder_Free(&builder);
                        return false;
                    }
                }
                break;
            }
            default:
                break;
        }
    }

    bool shouldClose = path->closed;
    if (!shouldClose && builder.count > 1) {
        Vec2 first = builder.points[0];
        Vec2 last = builder.points[builder.count - 1];
        if (Vec2_NearlyEqual(first, last, fmaxf(maxError, 0.001f))) {
            shouldClose = true;
        }
    }

    if (shouldClose && builder.count > 1) {
        builder.points[builder.count - 1] = builder.points[0];
    }

    out->points = builder.points;
    out->count = builder.count;
    out->closed = shouldClose;
    return true;
}

bool Shape_FlattenToPolylines(const Shape* shape,
                              float maxError,
                              PolylineSet* out) {
    if (!shape || !out) return false;
    memset(out, 0, sizeof(*out));

    if (shape->pathCount == 0) {
        return true;
    }

    Polyline* lines = (Polyline*)calloc(shape->pathCount, sizeof(Polyline));
    if (!lines) return false;

    for (size_t pi = 0; pi < shape->pathCount; ++pi) {
        if (!FlattenPath(&shape->paths[pi], maxError, &lines[pi])) {
            // clean up any polylines built so far
            for (size_t j = 0; j < pi; ++j) {
                free(lines[j].points);
            }
            free(lines);
            return false;
        }
    }

    out->lines = lines;
    out->count = shape->pathCount;
    return true;
}

void PolylineSet_Free(PolylineSet* set) {
    if (!set || !set->lines) return;
    for (size_t i = 0; i < set->count; ++i) {
        free(set->lines[i].points);
        set->lines[i].points = NULL;
        set->lines[i].count = 0;
        set->lines[i].closed = false;
    }
    free(set->lines);
    set->lines = NULL;
    set->count = 0;
}
