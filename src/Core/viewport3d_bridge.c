#include "Core/viewport3d_bridge.h"

#include <float.h>
#include <math.h>

static const double LINE_DRAWING_VIEWPORT3D_PI =
    3.14159265358979323846264338327950288;

bool LineDrawingViewport3DBridgeStateFromRuntime(
    const FreeViewCamera* camera, const Grid* grid,
    double viewport_center_x, double viewport_center_y,
    double min_grid_scale, double max_grid_scale,
    CoreViewport3DState* out_state) {
    CoreViewport3DOrientation orientation;
    CoreViewport3DBasis basis;
    CoreViewport3DVec3d effective_target;
    double scale;
    double offset_right;
    double offset_down;
    if (!camera || !grid || !out_state ||
        !isfinite(viewport_center_x) || !isfinite(viewport_center_y) ||
        !isfinite(grid->gridSize) || grid->gridSize <= 0.0f ||
        !isfinite(grid->scale) || grid->scale <= 0.0f ||
        !isfinite(grid->offsetX) || !isfinite(grid->offsetY) ||
        !isfinite(min_grid_scale) || min_grid_scale <= 0.0 ||
        !isfinite(max_grid_scale) || max_grid_scale < min_grid_scale ||
        !isfinite(camera->target.x) || !isfinite(camera->target.y) ||
        !isfinite(camera->target.z) ||
        !isfinite(camera->yawDeg) || !isfinite(camera->pitchDeg)) return false;
    scale = (double)grid->gridSize * (double)grid->scale;
    orientation.azimuth_rad = (double)camera->yawDeg * LINE_DRAWING_VIEWPORT3D_PI / 180.0;
    orientation.elevation_rad = (double)camera->pitchDeg * LINE_DRAWING_VIEWPORT3D_PI / 180.0;
    if (core_viewport3d_build_basis(&orientation, &basis).code != CORE_OK) return false;
    offset_right = (double)grid->offsetX + viewport_center_x / scale;
    offset_down = (double)grid->offsetY + viewport_center_y / scale;
    effective_target.x = (double)camera->target.x + basis.right.x * offset_right +
                         basis.screen_down.x * offset_down;
    effective_target.y = (double)camera->target.y + basis.right.y * offset_right +
                         basis.screen_down.y * offset_down;
    effective_target.z = (double)camera->target.z + basis.right.z * offset_right +
                         basis.screen_down.z * offset_down;
    return core_viewport3d_state_init(out_state,
                                      effective_target,
                                      orientation,
                                      scale,
                                      (double)grid->gridSize * min_grid_scale,
                                      (double)grid->gridSize * max_grid_scale).code == CORE_OK;
}

bool LineDrawingViewport3DBridgeCommit(
    const CoreViewport3DState* shared_state,
    double viewport_center_x, double viewport_center_y,
    FreeViewCamera* camera, Grid* grid) {
    FreeViewCamera candidate_camera;
    Grid candidate_grid;
    double grid_scale;
    if (!camera || !grid || !isfinite(viewport_center_x) || !isfinite(viewport_center_y) ||
        !isfinite(grid->gridSize) || grid->gridSize <= 0.0f ||
        core_viewport3d_state_validate(shared_state).code != CORE_OK) return false;
    grid_scale = shared_state->scale_px_per_world_unit / (double)grid->gridSize;
    if (!isfinite(grid_scale) || grid_scale <= 0.0 || grid_scale > (double)FLT_MAX ||
        fabs(shared_state->target.x) > (double)FLT_MAX ||
        fabs(shared_state->target.y) > (double)FLT_MAX ||
        fabs(shared_state->target.z) > (double)FLT_MAX) return false;
    candidate_camera = *camera;
    candidate_grid = *grid;
    candidate_camera.target = (Vec3){(float)shared_state->target.x,
                                     (float)shared_state->target.y,
                                     (float)shared_state->target.z};
    candidate_camera.yawDeg = (float)(shared_state->orientation.azimuth_rad *
                                      180.0 / LINE_DRAWING_VIEWPORT3D_PI);
    if (candidate_camera.yawDeg < 0.0f) candidate_camera.yawDeg += 360.0f;
    candidate_camera.pitchDeg = (float)(shared_state->orientation.elevation_rad *
                                        180.0 / LINE_DRAWING_VIEWPORT3D_PI);
    candidate_grid.scale = (float)grid_scale;
    candidate_grid.offsetX = (float)(-viewport_center_x /
                                     shared_state->scale_px_per_world_unit);
    candidate_grid.offsetY = (float)(-viewport_center_y /
                                     shared_state->scale_px_per_world_unit);
    if (!isfinite(candidate_grid.offsetX) || !isfinite(candidate_grid.offsetY)) return false;
    *camera = candidate_camera;
    *grid = candidate_grid;
    return true;
}

bool LineDrawingViewport3DBridgeApply(
    const FreeViewCamera* camera, const Grid* grid,
    double viewport_center_x, double viewport_center_y,
    double min_grid_scale, double max_grid_scale,
    const CoreViewport3DCommand* command,
    FreeViewCamera* out_camera, Grid* out_grid) {
    CoreViewport3DState before;
    CoreViewport3DState after;
    FreeViewCamera candidate_camera;
    Grid candidate_grid;
    if (!camera || !grid || !command || !out_camera || !out_grid ||
        !LineDrawingViewport3DBridgeStateFromRuntime(camera, grid,
                                                     viewport_center_x, viewport_center_y,
                                                     min_grid_scale, max_grid_scale, &before) ||
        core_viewport3d_apply(&before, command, &after).code != CORE_OK) return false;
    candidate_camera = *camera;
    candidate_grid = *grid;
    if (!LineDrawingViewport3DBridgeCommit(&after,
                                           viewport_center_x, viewport_center_y,
                                           &candidate_camera, &candidate_grid)) return false;
    /* LineDrawing's orbit contract treats the durable camera target and Grid
       offsets as independent storage oracles.  Re-expressing the unchanged
       shared effective target after a basis change would rewrite both even
       though orbit is orientation-only. */
    if (command->kind == CORE_VIEWPORT3D_COMMAND_ORBIT) {
        candidate_camera.target = camera->target;
        candidate_grid = *grid;
    }
    *out_camera = candidate_camera;
    *out_grid = candidate_grid;
    return true;
}

bool LineDrawingViewport3DBridgeApplyResize(
    const FreeViewCamera* camera, const Grid* grid,
    double old_viewport_center_x, double old_viewport_center_y,
    double new_viewport_center_x, double new_viewport_center_y,
    double min_grid_scale, double max_grid_scale,
    FreeViewCamera* out_camera, Grid* out_grid) {
    CoreViewport3DState before;
    CoreViewport3DState after;
    CoreViewport3DCommand command = {0};
    FreeViewCamera candidate_camera;
    Grid candidate_grid;
    if (!camera || !grid || !out_camera || !out_grid ||
        !LineDrawingViewport3DBridgeStateFromRuntime(
            camera, grid,
            old_viewport_center_x, old_viewport_center_y,
            min_grid_scale, max_grid_scale, &before)) {
        return false;
    }
    command.kind = CORE_VIEWPORT3D_COMMAND_RESIZE;
    if (core_viewport3d_apply(&before, &command, &after).code != CORE_OK) return false;
    candidate_camera = *camera;
    candidate_grid = *grid;
    if (!LineDrawingViewport3DBridgeCommit(&after,
                                           new_viewport_center_x,
                                           new_viewport_center_y,
                                           &candidate_camera,
                                           &candidate_grid)) {
        return false;
    }
    *out_camera = candidate_camera;
    *out_grid = candidate_grid;
    return true;
}
