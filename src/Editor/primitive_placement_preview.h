#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "Core/global_state.h"
#include "Editor/editor.h"
#include "Math/math_util.h"

typedef struct PrimitivePlacementPreview {
    PrimitivePlacementPreviewKind kind;
    PlaneFrame3 frame;
    float width;
    float height;
    float depth;
} PrimitivePlacementPreview;

bool Editor_PrimitivePlacementPreview_Build(const GlobalState* state,
                                            PrimitivePlacementPreviewKind kind,
                                            PrimitivePlacementPreview* outPreview);
bool Editor_PrimitivePlacementPreview_ComputeCorners(const PrimitivePlacementPreview* preview,
                                                     Vec3 outCorners[8],
                                                     size_t* outCornerCount);
