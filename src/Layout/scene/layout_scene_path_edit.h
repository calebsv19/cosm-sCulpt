#pragma once

#include "Layout/scene/layout_scene_authoring.h"

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    LINE_DRAWING_SCENE_PATH_ELEMENT_NONE = 0,
    LINE_DRAWING_SCENE_PATH_ELEMENT_ANCHOR = 1,
    LINE_DRAWING_SCENE_PATH_ELEMENT_INCOMING_TANGENT = 2,
    LINE_DRAWING_SCENE_PATH_ELEMENT_OUTGOING_TANGENT = 3,
    LINE_DRAWING_SCENE_PATH_ELEMENT_SEGMENT = 4
} LineDrawingScenePathElementKind;

typedef struct {
    LineDrawingScenePathElementKind kind;
    size_t control_index;
    size_t anchor_index;
    size_t segment_index;
} LineDrawingScenePathElementRef;

LineDrawingScenePathElementRef Layout_ScenePathEdit_ElementForControl(
    const LineDrawingScenePath* path,
    size_t control_index);
LineDrawingScenePathElementRef Layout_ScenePathEdit_Segment(size_t segment_index);
bool Layout_ScenePathEdit_ElementIsDraggable(LineDrawingScenePathElementRef element);
const char* Layout_ScenePathEdit_ElementKindName(LineDrawingScenePathElementKind kind);
size_t Layout_ScenePathEdit_AnchorCount(const LineDrawingScenePath* path);
LineDrawingScenePathTangentMode Layout_ScenePathEdit_AnchorMode(
    const LineDrawingScenePath* path,
    size_t anchor_index);
bool Layout_ScenePathEdit_SetAnchorMode(LineDrawingScenePath* path,
                                        size_t anchor_index,
                                        LineDrawingScenePathTangentMode mode);
bool Layout_ScenePathEdit_CycleAnchorMode(LineDrawingScenePath* path,
                                          size_t anchor_index);
bool Layout_ScenePathEdit_SetElementWorldPoint(LineDrawingScenePath* path,
                                               LineDrawingScenePathElementRef element,
                                               Vec3 point);
bool Layout_ScenePathEdit_SplitSegment(LineDrawingScenePath* path,
                                       size_t segment_index,
                                       float t,
                                       LineDrawingScenePathElementRef* out_anchor);
bool Layout_ScenePathEdit_DeleteElement(LineDrawingScenePath* path,
                                        LineDrawingScenePathElementRef element,
                                        LineDrawingScenePathElementRef* out_selection);
void Layout_ScenePathEdit_NormalizeModes(LineDrawingScenePath* path);
