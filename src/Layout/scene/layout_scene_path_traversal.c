#include "Layout/scene/layout_scene_path_traversal.h"

#include <math.h>
#include <string.h>

static Vec3 traversal_lerp(Vec3 a, Vec3 b, float t) {
    return (Vec3){a.x + (b.x - a.x) * t,
                  a.y + (b.y - a.y) * t,
                  a.z + (b.z - a.z) * t};
}

bool Layout_ScenePathTraversal_Build(const LineDrawingScenePath* path,
                                     LineDrawingScenePathTraversalTable* out_table) {
    LineDrawingScenePathGeometry geometry = {0};
    if (!out_table) return false;
    memset(out_table, 0, sizeof(*out_table));
    if (!path || !Layout_ScenePathGeometry_Build(path, &geometry) ||
        geometry.sample_count == 0u) return false;
    out_table->closed = path->closed;
    for (size_t i = 0u; i < geometry.sample_count; ++i) {
        out_table->samples[i] = geometry.samples[i];
        if (i > 0u) {
            out_table->total_distance +=
                Vec3_Length(Vec3_Sub(geometry.samples[i].world,
                                     geometry.samples[i - 1u].world));
        }
        out_table->cumulative_distance[i] = out_table->total_distance;
    }
    out_table->sample_count = geometry.sample_count;
    if (path->closed && geometry.sample_count > 1u &&
        out_table->sample_count < LINE_DRAWING_SCENE_PATH_TRAVERSAL_MAX_SAMPLES) {
        const size_t last = out_table->sample_count;
        out_table->total_distance +=
            Vec3_Length(Vec3_Sub(geometry.samples[0].world,
                                 geometry.samples[geometry.sample_count - 1u].world));
        out_table->samples[last] = geometry.samples[0];
        out_table->samples[last].source_segment = geometry.source_segment_count;
        out_table->samples[last].segment_t = 1.0f;
        out_table->cumulative_distance[last] = out_table->total_distance;
        out_table->sample_count++;
    }
    return true;
}

static float traversal_resolve_distance(float distance,
                                        float total,
                                        LineDrawingScenePathPlaybackMode mode) {
    if (!isfinite(distance) || total <= 0.0f) return 0.0f;
    if (mode == LINE_DRAWING_SCENE_PATH_PLAYBACK_LOOP) {
        float wrapped = fmodf(distance, total);
        if (wrapped < 0.0f) wrapped += total;
        return wrapped;
    }
    if (distance < 0.0f) return 0.0f;
    return distance > total ? total : distance;
}

bool Layout_ScenePathTraversal_EvaluateDistance(
    const LineDrawingScenePathTraversalTable* table,
    float distance,
    LineDrawingScenePathPlaybackMode playback_mode,
    LineDrawingScenePathTraversalSample* out_sample) {
    float target = 0.0f;
    if (!table || !out_sample || table->sample_count == 0u) return false;
    memset(out_sample, 0, sizeof(*out_sample));
    target = traversal_resolve_distance(distance, table->total_distance, playback_mode);
    if (table->sample_count == 1u || table->total_distance <= 0.000001f) {
        out_sample->world = table->samples[0].world;
        return true;
    }
    for (size_t i = 1u; i < table->sample_count; ++i) {
        if (target <= table->cumulative_distance[i] || i + 1u == table->sample_count) {
            const float a_distance = table->cumulative_distance[i - 1u];
            const float span = table->cumulative_distance[i] - a_distance;
            const float t = span > 0.000001f ? (target - a_distance) / span : 0.0f;
            out_sample->world = traversal_lerp(table->samples[i - 1u].world,
                                               table->samples[i].world, t);
            out_sample->source_segment = table->samples[i - 1u].source_segment;
            out_sample->segment_t = table->samples[i - 1u].segment_t +
                (table->samples[i].segment_t - table->samples[i - 1u].segment_t) * t;
            out_sample->distance = target;
            out_sample->normalized_distance = target / table->total_distance;
            return true;
        }
    }
    return false;
}

bool Layout_ScenePathTraversal_EvaluateNormalized(
    const LineDrawingScenePathTraversalTable* table,
    float normalized_distance,
    LineDrawingScenePathPlaybackMode playback_mode,
    LineDrawingScenePathTraversalSample* out_sample) {
    if (!table) return false;
    return Layout_ScenePathTraversal_EvaluateDistance(
        table, normalized_distance * table->total_distance, playback_mode, out_sample);
}

bool Layout_ScenePathTraversal_EvaluateTime(
    const LineDrawingScenePathTraversalTable* table,
    float time_seconds,
    float duration_seconds,
    LineDrawingScenePathPlaybackMode playback_mode,
    LineDrawingScenePathTraversalSample* out_sample) {
    if (!table || !isfinite(duration_seconds) || duration_seconds <= 0.0f) return false;
    return Layout_ScenePathTraversal_EvaluateNormalized(
        table, time_seconds / duration_seconds, playback_mode, out_sample);
}

bool Layout_ScenePathTraversal_Advance(LineDrawingScenePath* path, float delta_seconds) {
    if (!path || !path->playing || !isfinite(delta_seconds) || delta_seconds <= 0.0f ||
        !isfinite(path->duration_seconds) || path->duration_seconds <= 0.0f) return false;
    path->normalized_distance += delta_seconds / path->duration_seconds;
    if (path->playback_mode == LINE_DRAWING_SCENE_PATH_PLAYBACK_LOOP) {
        path->normalized_distance = fmodf(path->normalized_distance, 1.0f);
        if (path->normalized_distance < 0.0f) path->normalized_distance += 1.0f;
    } else if (path->normalized_distance >= 1.0f) {
        path->normalized_distance = 1.0f;
        path->playing = false;
    }
    return true;
}
