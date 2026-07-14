#pragma once

#include "Layout/scene/layout_scene_authoring.h"

#include <stdbool.h>
#include <stddef.h>

#define LINE_DRAWING_SCENE_PATH_CUBIC_SAMPLES_PER_SEGMENT 24u
#define LINE_DRAWING_SCENE_PATH_MAX_SAMPLES 128u

typedef enum {
    LINE_DRAWING_SCENE_PATH_GEOMETRY_EMPTY = 0,
    LINE_DRAWING_SCENE_PATH_GEOMETRY_LINEAR = 1,
    LINE_DRAWING_SCENE_PATH_GEOMETRY_CUBIC_BEZIER = 2,
    LINE_DRAWING_SCENE_PATH_GEOMETRY_INCOMPLETE_BEZIER_FALLBACK = 3
} LineDrawingScenePathGeometryKind;

typedef struct {
    Vec3 world;
    size_t source_segment;
    float segment_t;
} LineDrawingScenePathSample;

typedef struct {
    LineDrawingScenePathGeometryKind kind;
    LineDrawingScenePathSample samples[LINE_DRAWING_SCENE_PATH_MAX_SAMPLES];
    size_t sample_count;
    size_t source_segment_count;
} LineDrawingScenePathGeometry;

bool Layout_ScenePathGeometry_IsCompleteCubic(const LineDrawingScenePath* path);
const char* Layout_ScenePathGeometry_KindName(LineDrawingScenePathGeometryKind kind);
Vec3 Layout_ScenePathGeometry_EvaluateCubic(Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, float t);
bool Layout_ScenePathGeometry_Build(const LineDrawingScenePath* path,
                                    LineDrawingScenePathGeometry* out_geometry);
bool Layout_ScenePathGeometry_SplitCubicSegment(LineDrawingScenePath* path,
                                                size_t segment_index,
                                                float t,
                                                size_t* out_inserted_anchor_index);
