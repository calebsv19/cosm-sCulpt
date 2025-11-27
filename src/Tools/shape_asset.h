// Tools/shape_asset.h
#pragma once

#include <stddef.h>
#include <stdbool.h>
#include "Math/math_util.h"
#include "Layout/layout.h"

// Basic segment type for 2D shapes reconstructed from a Layout.
//
// This is intentionally minimal and local to the Tools pipeline for now.
// Later you can move this into a shared library if you want to reuse it
// from multiple projects.

typedef enum {
    SHAPE_SEGMENT_LINE = 0,
    SHAPE_SEGMENT_CUBIC_BEZIER = 1
} ShapeSegmentType;

typedef struct {
    ShapeSegmentType type;

    // Start and end points in world space.
    Vec2 p0;
    Vec2 p1;

    // Cubic Bezier control points (only used when type == SHAPE_SEGMENT_CUBIC_BEZIER).
    Vec2 c1;
    Vec2 c2;
} ShapeSegment;

typedef struct {
    bool   closed;
    size_t segmentCount;
    ShapeSegment* segments;
} ShapePath;

typedef struct {
    const char* name; // optional; can point at a static string or the input file name
    size_t      pathCount;
    ShapePath*  paths;
} Shape;

typedef struct {
    size_t shapeCount;
    Shape* shapes;
} ShapeDocument;

// Convert a Layout (anchors + walls) into a ShapeDocument.
// For now we treat the entire Layout as a single Shape containing
// one or more ShapePaths (connected components of the wall graph).
//
// Returns true on success; on failure, the output is left in a
// freed/empty state.
bool ShapeDocument_FromLayout(const char* name,
                              const Layout* layout,
                              ShapeDocument* outDoc);

// Free all dynamically allocated memory inside the document.
void ShapeDocument_Free(ShapeDocument* doc);
