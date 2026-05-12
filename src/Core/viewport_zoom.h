#pragma once

#include <stdbool.h>

#include "Core/space_mode_adapter.h"
#include "Layout/Grid/grid.h"
#include "Layout/layout.h"

typedef struct GlobalState GlobalState;

float LineDrawingViewportZoom_MinScaleForSceneBounds(const SceneBounds3D* bounds,
                                                     const SpaceViewContext* view,
                                                     float viewportWidth,
                                                     float viewportHeight,
                                                     float gridSize);

bool LineDrawingViewportZoom_Apply(GlobalState* state,
                                   float zoomFactor,
                                   float anchorScreenX,
                                   float anchorScreenY);
