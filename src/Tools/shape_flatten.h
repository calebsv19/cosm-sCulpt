// src/Tools/shape_flatten.h
#pragma once

#include <stddef.h>
#include <stdbool.h>
#include "Tools/shape_asset.h"

typedef struct {
    Vec2*  points;
    size_t count;
    bool   closed;
} Polyline;

typedef struct {
    Polyline* lines;
    size_t    count;
} PolylineSet;

// Flatten all paths of a shape into polylines by subdividing cubic segments
// until their deviation from the chord is below maxError.
bool Shape_FlattenToPolylines(const Shape* shape,
                              float maxError,
                              PolylineSet* out);

// Release memory allocated inside the polyline set.
void PolylineSet_Free(PolylineSet* set);
