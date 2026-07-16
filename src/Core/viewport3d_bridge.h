#pragma once

#include "Layout/Grid/grid.h"
#include "Math/math_util.h"
#include "core_viewport3d.h"

#include <stdbool.h>

bool LineDrawingViewport3DBridgeStateFromRuntime(
    const FreeViewCamera* camera, const Grid* grid,
    double viewport_center_x, double viewport_center_y,
    double min_grid_scale, double max_grid_scale,
    CoreViewport3DState* out_state);
bool LineDrawingViewport3DBridgeCommit(
    const CoreViewport3DState* shared_state,
    double viewport_center_x, double viewport_center_y,
    FreeViewCamera* camera, Grid* grid);
bool LineDrawingViewport3DBridgeApply(
    const FreeViewCamera* camera, const Grid* grid,
    double viewport_center_x, double viewport_center_y,
    double min_grid_scale, double max_grid_scale,
    const CoreViewport3DCommand* command,
    FreeViewCamera* out_camera, Grid* out_grid);
bool LineDrawingViewport3DBridgeApplyResize(
    const FreeViewCamera* camera, const Grid* grid,
    double old_viewport_center_x, double old_viewport_center_y,
    double new_viewport_center_x, double new_viewport_center_y,
    double min_grid_scale, double max_grid_scale,
    FreeViewCamera* out_camera, Grid* out_grid);
