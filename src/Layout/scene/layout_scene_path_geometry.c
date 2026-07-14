#include "Layout/scene/layout_scene_path_geometry.h"

#include <string.h>

static Vec3 ld_path_lerp(Vec3 a, Vec3 b, float t) {
    return (Vec3){
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    };
}

bool Layout_ScenePathGeometry_IsCompleteCubic(const LineDrawingScenePath* path) {
    return path && path->control_point_count >= 4u &&
           ((path->control_point_count - 1u) % 3u) == 0u;
}

const char* Layout_ScenePathGeometry_KindName(LineDrawingScenePathGeometryKind kind) {
    switch (kind) {
        case LINE_DRAWING_SCENE_PATH_GEOMETRY_LINEAR: return "linear";
        case LINE_DRAWING_SCENE_PATH_GEOMETRY_CUBIC_BEZIER: return "cubic-bezier";
        case LINE_DRAWING_SCENE_PATH_GEOMETRY_INCOMPLETE_BEZIER_FALLBACK:
            return "bezier-fallback";
        case LINE_DRAWING_SCENE_PATH_GEOMETRY_EMPTY:
        default: return "empty";
    }
}

Vec3 Layout_ScenePathGeometry_EvaluateCubic(Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, float t) {
    const float clamped = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
    const float u = 1.0f - clamped;
    const float uu = u * u;
    const float tt = clamped * clamped;
    const float a = uu * u;
    const float b = 3.0f * uu * clamped;
    const float c = 3.0f * u * tt;
    const float d = tt * clamped;
    return (Vec3){
        a * p0.x + b * p1.x + c * p2.x + d * p3.x,
        a * p0.y + b * p1.y + c * p2.y + d * p3.y,
        a * p0.z + b * p1.z + c * p2.z + d * p3.z
    };
}

static bool ld_path_geometry_build_linear(const LineDrawingScenePath* path,
                                          LineDrawingScenePathGeometry* geometry,
                                          LineDrawingScenePathGeometryKind kind) {
    geometry->kind = kind;
    geometry->source_segment_count = path->control_point_count > 1u
        ? path->control_point_count - 1u
        : 0u;
    for (size_t i = 0u; i < path->control_point_count; ++i) {
        if (geometry->sample_count >= LINE_DRAWING_SCENE_PATH_MAX_SAMPLES) return false;
        geometry->samples[geometry->sample_count++] = (LineDrawingScenePathSample){
            .world = path->control_points[i],
            .source_segment = i > 0u ? i - 1u : 0u,
            .segment_t = i > 0u ? 1.0f : 0.0f
        };
    }
    return geometry->sample_count > 0u;
}

bool Layout_ScenePathGeometry_Build(const LineDrawingScenePath* path,
                                    LineDrawingScenePathGeometry* out_geometry) {
    if (!out_geometry) return false;
    memset(out_geometry, 0, sizeof(*out_geometry));
    if (!path || path->control_point_count == 0u) return false;
    if (strcmp(path->curve_type, "bezier") != 0) {
        return ld_path_geometry_build_linear(path,
                                             out_geometry,
                                             LINE_DRAWING_SCENE_PATH_GEOMETRY_LINEAR);
    }
    if (!Layout_ScenePathGeometry_IsCompleteCubic(path)) {
        return ld_path_geometry_build_linear(
            path,
            out_geometry,
            LINE_DRAWING_SCENE_PATH_GEOMETRY_INCOMPLETE_BEZIER_FALLBACK);
    }

    out_geometry->kind = LINE_DRAWING_SCENE_PATH_GEOMETRY_CUBIC_BEZIER;
    out_geometry->source_segment_count = (path->control_point_count - 1u) / 3u;
    for (size_t segment = 0u; segment < out_geometry->source_segment_count; ++segment) {
        const size_t base = segment * 3u;
        const size_t first_step = segment == 0u ? 0u : 1u;
        for (size_t step = first_step;
             step <= LINE_DRAWING_SCENE_PATH_CUBIC_SAMPLES_PER_SEGMENT;
             ++step) {
            if (out_geometry->sample_count >= LINE_DRAWING_SCENE_PATH_MAX_SAMPLES) return false;
            const float t = (float)step /
                (float)LINE_DRAWING_SCENE_PATH_CUBIC_SAMPLES_PER_SEGMENT;
            out_geometry->samples[out_geometry->sample_count++] =
                (LineDrawingScenePathSample){
                    .world = Layout_ScenePathGeometry_EvaluateCubic(
                        path->control_points[base],
                        path->control_points[base + 1u],
                        path->control_points[base + 2u],
                        path->control_points[base + 3u],
                        t),
                    .source_segment = segment,
                    .segment_t = t
                };
        }
    }
    return out_geometry->sample_count > 0u;
}

bool Layout_ScenePathGeometry_SplitCubicSegment(LineDrawingScenePath* path,
                                                size_t segment_index,
                                                float t,
                                                size_t* out_inserted_anchor_index) {
    Vec3 p0 = {0};
    Vec3 p1 = {0};
    Vec3 p2 = {0};
    Vec3 p3 = {0};
    Vec3 a = {0};
    Vec3 b = {0};
    Vec3 c = {0};
    Vec3 d = {0};
    Vec3 e = {0};
    Vec3 split = {0};
    size_t base = 0u;
    if (out_inserted_anchor_index) *out_inserted_anchor_index = 0u;
    if (!Layout_ScenePathGeometry_IsCompleteCubic(path) || t <= 0.0f || t >= 1.0f ||
        path->control_point_count + 3u > LINE_DRAWING_SCENE_AUTHORING_MAX_PATH_POINTS ||
        segment_index >= (path->control_point_count - 1u) / 3u) {
        return false;
    }
    base = segment_index * 3u;
    p0 = path->control_points[base];
    p1 = path->control_points[base + 1u];
    p2 = path->control_points[base + 2u];
    p3 = path->control_points[base + 3u];
    a = ld_path_lerp(p0, p1, t);
    b = ld_path_lerp(p1, p2, t);
    c = ld_path_lerp(p2, p3, t);
    d = ld_path_lerp(a, b, t);
    e = ld_path_lerp(b, c, t);
    split = ld_path_lerp(d, e, t);

    for (size_t i = path->control_point_count; i > base + 3u; --i) {
        path->control_points[i + 2u] = path->control_points[i - 1u];
    }
    path->control_points[base + 1u] = a;
    path->control_points[base + 2u] = d;
    path->control_points[base + 3u] = split;
    path->control_points[base + 4u] = e;
    path->control_points[base + 5u] = c;
    path->control_points[base + 6u] = p3;
    path->control_point_count += 3u;
    if (out_inserted_anchor_index) *out_inserted_anchor_index = base + 3u;
    return true;
}
