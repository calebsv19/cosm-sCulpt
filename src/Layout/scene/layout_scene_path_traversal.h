#pragma once

#include "Layout/scene/layout_scene_path_geometry.h"

#define LINE_DRAWING_SCENE_PATH_TRAVERSAL_MAX_SAMPLES \
    (LINE_DRAWING_SCENE_PATH_MAX_SAMPLES + 1u)

typedef struct {
    LineDrawingScenePathSample samples[LINE_DRAWING_SCENE_PATH_TRAVERSAL_MAX_SAMPLES];
    float cumulative_distance[LINE_DRAWING_SCENE_PATH_TRAVERSAL_MAX_SAMPLES];
    size_t sample_count;
    float total_distance;
    bool closed;
} LineDrawingScenePathTraversalTable;

typedef struct {
    Vec3 world;
    size_t source_segment;
    float segment_t;
    float distance;
    float normalized_distance;
} LineDrawingScenePathTraversalSample;

bool Layout_ScenePathTraversal_Build(const LineDrawingScenePath* path,
                                     LineDrawingScenePathTraversalTable* out_table);
bool Layout_ScenePathTraversal_EvaluateDistance(
    const LineDrawingScenePathTraversalTable* table,
    float distance,
    LineDrawingScenePathPlaybackMode playback_mode,
    LineDrawingScenePathTraversalSample* out_sample);
bool Layout_ScenePathTraversal_EvaluateNormalized(
    const LineDrawingScenePathTraversalTable* table,
    float normalized_distance,
    LineDrawingScenePathPlaybackMode playback_mode,
    LineDrawingScenePathTraversalSample* out_sample);
bool Layout_ScenePathTraversal_EvaluateTime(
    const LineDrawingScenePathTraversalTable* table,
    float time_seconds,
    float duration_seconds,
    LineDrawingScenePathPlaybackMode playback_mode,
    LineDrawingScenePathTraversalSample* out_sample);
bool Layout_ScenePathTraversal_Advance(LineDrawingScenePath* path, float delta_seconds);
