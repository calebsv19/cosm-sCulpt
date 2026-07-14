#include "Layout/scene/layout_scene_path_edit.h"

#include "Layout/scene/layout_scene_path_geometry.h"

#include <math.h>
#include <string.h>

static Vec3 ld_path_add(Vec3 a, Vec3 b) {
    return (Vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static Vec3 ld_path_sub(Vec3 a, Vec3 b) {
    return (Vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static Vec3 ld_path_scale(Vec3 value, float scale) {
    return (Vec3){value.x * scale, value.y * scale, value.z * scale};
}

static float ld_path_length(Vec3 value) {
    return sqrtf(value.x * value.x + value.y * value.y + value.z * value.z);
}

static Vec3 ld_path_direction(Vec3 value, Vec3 fallback) {
    const float length = ld_path_length(value);
    return length > 0.00001f ? ld_path_scale(value, 1.0f / length) : fallback;
}

size_t Layout_ScenePathEdit_AnchorCount(const LineDrawingScenePath* path) {
    if (!path || path->control_point_count == 0u) return 0u;
    if (Layout_ScenePathGeometry_IsCompleteCubic(path)) {
        return ((path->control_point_count - 1u) / 3u) + 1u;
    }
    return path->control_point_count;
}

LineDrawingScenePathElementRef Layout_ScenePathEdit_ElementForControl(
    const LineDrawingScenePath* path,
    size_t control_index) {
    LineDrawingScenePathElementRef result = {0};
    if (!path || control_index >= path->control_point_count) return result;
    result.control_index = control_index;
    if (!Layout_ScenePathGeometry_IsCompleteCubic(path)) {
        result.kind = LINE_DRAWING_SCENE_PATH_ELEMENT_ANCHOR;
        result.anchor_index = control_index;
        return result;
    }
    if ((control_index % 3u) == 0u) {
        result.kind = LINE_DRAWING_SCENE_PATH_ELEMENT_ANCHOR;
        result.anchor_index = control_index / 3u;
    } else if ((control_index % 3u) == 1u) {
        result.kind = LINE_DRAWING_SCENE_PATH_ELEMENT_OUTGOING_TANGENT;
        result.anchor_index = control_index / 3u;
    } else {
        result.kind = LINE_DRAWING_SCENE_PATH_ELEMENT_INCOMING_TANGENT;
        result.anchor_index = (control_index + 1u) / 3u;
    }
    return result;
}

LineDrawingScenePathElementRef Layout_ScenePathEdit_Segment(size_t segment_index) {
    return (LineDrawingScenePathElementRef){
        .kind = LINE_DRAWING_SCENE_PATH_ELEMENT_SEGMENT,
        .segment_index = segment_index
    };
}

bool Layout_ScenePathEdit_ElementIsDraggable(LineDrawingScenePathElementRef element) {
    return element.kind == LINE_DRAWING_SCENE_PATH_ELEMENT_ANCHOR ||
           element.kind == LINE_DRAWING_SCENE_PATH_ELEMENT_INCOMING_TANGENT ||
           element.kind == LINE_DRAWING_SCENE_PATH_ELEMENT_OUTGOING_TANGENT;
}

const char* Layout_ScenePathEdit_ElementKindName(LineDrawingScenePathElementKind kind) {
    switch (kind) {
        case LINE_DRAWING_SCENE_PATH_ELEMENT_ANCHOR: return "anchor";
        case LINE_DRAWING_SCENE_PATH_ELEMENT_INCOMING_TANGENT: return "incoming tangent";
        case LINE_DRAWING_SCENE_PATH_ELEMENT_OUTGOING_TANGENT: return "outgoing tangent";
        case LINE_DRAWING_SCENE_PATH_ELEMENT_SEGMENT: return "segment";
        case LINE_DRAWING_SCENE_PATH_ELEMENT_NONE:
        default: return "none";
    }
}

LineDrawingScenePathTangentMode Layout_ScenePathEdit_AnchorMode(
    const LineDrawingScenePath* path,
    size_t anchor_index) {
    if (!path || anchor_index >= Layout_ScenePathEdit_AnchorCount(path) ||
        anchor_index >= LINE_DRAWING_SCENE_AUTHORING_MAX_PATH_ANCHORS) {
        return LINE_DRAWING_SCENE_PATH_TANGENT_BROKEN;
    }
    return path->tangent_modes[anchor_index];
}

static void ld_path_apply_corner(LineDrawingScenePath* path, size_t anchor_index) {
    const size_t control = anchor_index * 3u;
    const Vec3 anchor = path->control_points[control];
    if (control > 0u) path->control_points[control - 1u] = anchor;
    if (control + 1u < path->control_point_count) path->control_points[control + 1u] = anchor;
}

static void ld_path_apply_automatic(LineDrawingScenePath* path, size_t anchor_index) {
    const size_t anchor_count = Layout_ScenePathEdit_AnchorCount(path);
    const size_t control = anchor_index * 3u;
    const Vec3 anchor = path->control_points[control];
    Vec3 direction = {1.0f, 0.0f, 0.0f};
    float incoming_length = 0.0f;
    float outgoing_length = 0.0f;
    if (anchor_count < 2u) return;
    if (anchor_index == 0u) {
        Vec3 next = path->control_points[3u];
        direction = ld_path_direction(ld_path_sub(next, anchor), direction);
        outgoing_length = ld_path_length(ld_path_sub(next, anchor)) / 3.0f;
    } else if (anchor_index + 1u == anchor_count) {
        Vec3 previous = path->control_points[control - 3u];
        direction = ld_path_direction(ld_path_sub(anchor, previous), direction);
        incoming_length = ld_path_length(ld_path_sub(anchor, previous)) / 3.0f;
    } else {
        Vec3 previous = path->control_points[control - 3u];
        Vec3 next = path->control_points[control + 3u];
        direction = ld_path_direction(ld_path_sub(next, previous), direction);
        incoming_length = ld_path_length(ld_path_sub(anchor, previous)) / 3.0f;
        outgoing_length = ld_path_length(ld_path_sub(next, anchor)) / 3.0f;
    }
    if (control > 0u) {
        path->control_points[control - 1u] =
            ld_path_sub(anchor, ld_path_scale(direction, incoming_length));
    }
    if (control + 1u < path->control_point_count) {
        path->control_points[control + 1u] =
            ld_path_add(anchor, ld_path_scale(direction, outgoing_length));
    }
}

static void ld_path_apply_mode(LineDrawingScenePath* path, size_t anchor_index) {
    const LineDrawingScenePathTangentMode mode = path->tangent_modes[anchor_index];
    if (mode == LINE_DRAWING_SCENE_PATH_TANGENT_CORNER) {
        ld_path_apply_corner(path, anchor_index);
    } else if (mode == LINE_DRAWING_SCENE_PATH_TANGENT_AUTOMATIC) {
        ld_path_apply_automatic(path, anchor_index);
    }
}

bool Layout_ScenePathEdit_SetAnchorMode(LineDrawingScenePath* path,
                                        size_t anchor_index,
                                        LineDrawingScenePathTangentMode mode) {
    if (!Layout_ScenePathGeometry_IsCompleteCubic(path) ||
        anchor_index >= Layout_ScenePathEdit_AnchorCount(path) ||
        mode < LINE_DRAWING_SCENE_PATH_TANGENT_LINKED ||
        mode > LINE_DRAWING_SCENE_PATH_TANGENT_CORNER) {
        return false;
    }
    path->tangent_modes[anchor_index] = mode;
    ld_path_apply_mode(path, anchor_index);
    return true;
}

bool Layout_ScenePathEdit_CycleAnchorMode(LineDrawingScenePath* path,
                                          size_t anchor_index) {
    LineDrawingScenePathTangentMode next = Layout_ScenePathEdit_AnchorMode(path, anchor_index);
    next = (LineDrawingScenePathTangentMode)(((int)next + 1) % 5);
    return Layout_ScenePathEdit_SetAnchorMode(path, anchor_index, next);
}

static void ld_path_move_opposite_tangent(LineDrawingScenePath* path,
                                          LineDrawingScenePathElementRef element,
                                          Vec3 moved) {
    const size_t anchor_control = element.anchor_index * 3u;
    const Vec3 anchor = path->control_points[anchor_control];
    const bool incoming = element.kind == LINE_DRAWING_SCENE_PATH_ELEMENT_INCOMING_TANGENT;
    const size_t opposite = incoming ? anchor_control + 1u : anchor_control - 1u;
    const LineDrawingScenePathTangentMode mode = path->tangent_modes[element.anchor_index];
    Vec3 direction = ld_path_direction(ld_path_sub(anchor, moved), (Vec3){1.0f, 0.0f, 0.0f});
    if (mode == LINE_DRAWING_SCENE_PATH_TANGENT_LINKED) {
        path->control_points[opposite] = ld_path_add(anchor, ld_path_sub(anchor, moved));
    } else if (mode == LINE_DRAWING_SCENE_PATH_TANGENT_SMOOTH) {
        const float opposite_length = ld_path_length(ld_path_sub(path->control_points[opposite], anchor));
        path->control_points[opposite] = ld_path_add(anchor, ld_path_scale(direction, opposite_length));
    }
}

bool Layout_ScenePathEdit_SetElementWorldPoint(LineDrawingScenePath* path,
                                               LineDrawingScenePathElementRef element,
                                               Vec3 point) {
    if (!path || !Layout_ScenePathEdit_ElementIsDraggable(element) ||
        element.control_index >= path->control_point_count) return false;
    if (!Layout_ScenePathGeometry_IsCompleteCubic(path)) {
        path->control_points[element.control_index] = point;
        return true;
    }
    if (element.kind == LINE_DRAWING_SCENE_PATH_ELEMENT_ANCHOR) {
        const Vec3 delta = ld_path_sub(point, path->control_points[element.control_index]);
        path->control_points[element.control_index] = point;
        if (element.control_index > 0u) {
            path->control_points[element.control_index - 1u] =
                ld_path_add(path->control_points[element.control_index - 1u], delta);
        }
        if (element.control_index + 1u < path->control_point_count) {
            path->control_points[element.control_index + 1u] =
                ld_path_add(path->control_points[element.control_index + 1u], delta);
        }
        if (element.anchor_index > 0u) ld_path_apply_mode(path, element.anchor_index - 1u);
        ld_path_apply_mode(path, element.anchor_index);
        if (element.anchor_index + 1u < Layout_ScenePathEdit_AnchorCount(path)) {
            ld_path_apply_mode(path, element.anchor_index + 1u);
        }
        return true;
    }
    if (path->tangent_modes[element.anchor_index] == LINE_DRAWING_SCENE_PATH_TANGENT_AUTOMATIC) {
        path->tangent_modes[element.anchor_index] = LINE_DRAWING_SCENE_PATH_TANGENT_SMOOTH;
    } else if (path->tangent_modes[element.anchor_index] == LINE_DRAWING_SCENE_PATH_TANGENT_CORNER) {
        path->tangent_modes[element.anchor_index] = LINE_DRAWING_SCENE_PATH_TANGENT_BROKEN;
    }
    path->control_points[element.control_index] = point;
    if (element.anchor_index > 0u &&
        element.anchor_index + 1u < Layout_ScenePathEdit_AnchorCount(path)) {
        ld_path_move_opposite_tangent(path, element, point);
    }
    return true;
}

bool Layout_ScenePathEdit_SplitSegment(LineDrawingScenePath* path,
                                       size_t segment_index,
                                       float t,
                                       LineDrawingScenePathElementRef* out_anchor) {
    LineDrawingScenePathTangentMode old_modes[LINE_DRAWING_SCENE_AUTHORING_MAX_PATH_ANCHORS];
    size_t old_anchor_count = 0u;
    size_t control_index = 0u;
    if (out_anchor) *out_anchor = (LineDrawingScenePathElementRef){0};
    if (!Layout_ScenePathGeometry_IsCompleteCubic(path)) return false;
    old_anchor_count = Layout_ScenePathEdit_AnchorCount(path);
    memcpy(old_modes, path->tangent_modes, sizeof(old_modes));
    if (!Layout_ScenePathGeometry_SplitCubicSegment(path, segment_index, t, &control_index)) {
        return false;
    }
    for (size_t i = old_anchor_count; i > segment_index + 1u; --i) {
        path->tangent_modes[i] = old_modes[i - 1u];
    }
    path->tangent_modes[segment_index + 1u] = LINE_DRAWING_SCENE_PATH_TANGENT_SMOOTH;
    if (out_anchor) {
        *out_anchor = Layout_ScenePathEdit_ElementForControl(path, control_index);
    }
    return true;
}

static void ld_path_remove_controls(LineDrawingScenePath* path, size_t first, size_t count) {
    for (size_t i = first + count; i < path->control_point_count; ++i) {
        path->control_points[i - count] = path->control_points[i];
    }
    path->control_point_count -= count;
}

bool Layout_ScenePathEdit_DeleteElement(LineDrawingScenePath* path,
                                        LineDrawingScenePathElementRef element,
                                        LineDrawingScenePathElementRef* out_selection) {
    size_t anchor_index = element.anchor_index;
    size_t anchor_count = Layout_ScenePathEdit_AnchorCount(path);
    size_t remove_first = 0u;
    if (out_selection) *out_selection = (LineDrawingScenePathElementRef){0};
    if (!path) return false;
    if (!Layout_ScenePathGeometry_IsCompleteCubic(path)) {
        if (element.kind != LINE_DRAWING_SCENE_PATH_ELEMENT_ANCHOR ||
            path->control_point_count <= 2u || element.control_index >= path->control_point_count) {
            return false;
        }
        ld_path_remove_controls(path, element.control_index, 1u);
        if (out_selection && path->control_point_count > 0u) {
            *out_selection = Layout_ScenePathEdit_ElementForControl(
                path, element.control_index < path->control_point_count
                          ? element.control_index : path->control_point_count - 1u);
        }
        return true;
    }
    if (element.kind == LINE_DRAWING_SCENE_PATH_ELEMENT_INCOMING_TANGENT ||
        element.kind == LINE_DRAWING_SCENE_PATH_ELEMENT_OUTGOING_TANGENT) {
        const size_t anchor_control = anchor_index * 3u;
        path->control_points[element.control_index] = path->control_points[anchor_control];
        path->tangent_modes[anchor_index] = LINE_DRAWING_SCENE_PATH_TANGENT_BROKEN;
        if (out_selection) *out_selection = Layout_ScenePathEdit_ElementForControl(path, anchor_control);
        return true;
    }
    if (element.kind == LINE_DRAWING_SCENE_PATH_ELEMENT_SEGMENT) {
        if (element.segment_index >= anchor_count - 1u) return false;
        anchor_index = element.segment_index + 1u;
    } else if (element.kind != LINE_DRAWING_SCENE_PATH_ELEMENT_ANCHOR) {
        return false;
    }
    if (anchor_count <= 2u || anchor_index >= anchor_count) return false;
    remove_first = anchor_index == 0u ? 0u :
                   anchor_index + 1u == anchor_count ? path->control_point_count - 3u :
                   anchor_index * 3u - 1u;
    ld_path_remove_controls(path, remove_first, 3u);
    for (size_t i = anchor_index; i + 1u < anchor_count; ++i) {
        path->tangent_modes[i] = path->tangent_modes[i + 1u];
    }
    --anchor_count;
    if (anchor_index >= anchor_count) anchor_index = anchor_count - 1u;
    if (out_selection) {
        *out_selection = Layout_ScenePathEdit_ElementForControl(path, anchor_index * 3u);
    }
    return true;
}

void Layout_ScenePathEdit_NormalizeModes(LineDrawingScenePath* path) {
    const size_t anchor_count = Layout_ScenePathEdit_AnchorCount(path);
    if (!path) return;
    for (size_t i = 0u; i < anchor_count; ++i) {
        if (path->tangent_modes[i] < LINE_DRAWING_SCENE_PATH_TANGENT_LINKED ||
            path->tangent_modes[i] > LINE_DRAWING_SCENE_PATH_TANGENT_CORNER) {
            path->tangent_modes[i] = LINE_DRAWING_SCENE_PATH_TANGENT_SMOOTH;
        }
    }
}
