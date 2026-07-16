#include "Core/viewport_zoom.h"

#include "Core/global_state.h"
#include "Core/line_drawing_pane_host.h"
#include "Core/viewport3d_bridge.h"
#include "Core/viewport_navigation_contract.h"

#include <float.h>
#include <math.h>

enum {
    LINE_DRAWING_ZOOM_FIT_PADDING_PERCENT = 84
};

static const float LINE_DRAWING_ZOOM_ABSOLUTE_MIN_SCALE = 0.01f;
static const float LINE_DRAWING_ZOOM_EXTENT_EPSILON = 0.0001f;

static bool LineDrawingViewportZoom_GetCenterViewport(const GlobalState* state,
                                                      CorePaneRect* out_viewport) {
    CorePaneRect viewport = {0};
    if (!state || !out_viewport) return false;
    if (state->paneHost.initialized &&
        LineDrawingPaneHost_GetViewportRect(&state->paneHost, &viewport) &&
        viewport.width > 1.0f &&
        viewport.height > 1.0f) {
        *out_viewport = viewport;
        return true;
    }
    *out_viewport = (CorePaneRect){0.0f, 0.0f,
                                   (float)state->screenWidth,
                                   (float)state->screenHeight};
    return out_viewport->width > 1.0f && out_viewport->height > 1.0f;
}

float LineDrawingViewportZoom_MinScaleForSceneBounds(const SceneBounds3D* bounds,
                                                     const SpaceViewContext* view,
                                                     float viewportWidth,
                                                     float viewportHeight,
                                                     float gridSize) {
    float minX = FLT_MAX;
    float minY = FLT_MAX;
    float maxX = -FLT_MAX;
    float maxY = -FLT_MAX;
    float spanX = 0.0f;
    float spanY = 0.0f;
    float usableW = 0.0f;
    float usableH = 0.0f;
    float scaleX = FLT_MAX;
    float scaleY = FLT_MAX;
    float fitScale = GRID_DEFAULT_MIN_SCALE;

    if (!bounds || !view || !bounds->enabled || !Layout_SceneBounds3D_IsValid(bounds)) {
        return GRID_DEFAULT_MIN_SCALE;
    }
    if (viewportWidth <= 1.0f || viewportHeight <= 1.0f || gridSize <= 0.0f) {
        return GRID_DEFAULT_MIN_SCALE;
    }

    for (int i = 0; i < 8; ++i) {
        Vec3 corner = {
            (i & 1) ? bounds->max.x : bounds->min.x,
            (i & 2) ? bounds->max.y : bounds->min.y,
            (i & 4) ? bounds->max.z : bounds->min.z
        };
        Vec2 projected = SpaceAdapter_ProjectToView(corner, view);
        if (projected.x < minX) minX = projected.x;
        if (projected.x > maxX) maxX = projected.x;
        if (projected.y < minY) minY = projected.y;
        if (projected.y > maxY) maxY = projected.y;
    }

    spanX = maxX - minX;
    spanY = maxY - minY;
    if (spanX <= LINE_DRAWING_ZOOM_EXTENT_EPSILON &&
        spanY <= LINE_DRAWING_ZOOM_EXTENT_EPSILON) {
        return GRID_DEFAULT_MIN_SCALE;
    }

    usableW = viewportWidth * ((float)LINE_DRAWING_ZOOM_FIT_PADDING_PERCENT / 100.0f);
    usableH = viewportHeight * ((float)LINE_DRAWING_ZOOM_FIT_PADDING_PERCENT / 100.0f);
    if (spanX > LINE_DRAWING_ZOOM_EXTENT_EPSILON) {
        scaleX = usableW / (gridSize * spanX);
    }
    if (spanY > LINE_DRAWING_ZOOM_EXTENT_EPSILON) {
        scaleY = usableH / (gridSize * spanY);
    }

    fitScale = fminf(scaleX, scaleY);
    if (!isfinite(fitScale) || fitScale <= 0.0f) {
        return GRID_DEFAULT_MIN_SCALE;
    }
    if (fitScale > GRID_DEFAULT_MIN_SCALE) {
        return GRID_DEFAULT_MIN_SCALE;
    }
    if (fitScale < LINE_DRAWING_ZOOM_ABSOLUTE_MIN_SCALE) {
        return LINE_DRAWING_ZOOM_ABSOLUTE_MIN_SCALE;
    }
    return fitScale;
}

bool LineDrawingViewportZoom_Apply(GlobalState* state,
                                   float zoomFactor,
                                   float anchorScreenX,
                                   float anchorScreenY) {
    SpaceViewContext view;
    CorePaneRect viewport = {0};
    float minScale = GRID_DEFAULT_MIN_SCALE;
    LineDrawingViewportNavState before = {0};
    LineDrawingViewportNavState after = {0};
    LineDrawingViewportNavCommand command = {0};

    if (!state) return false;
    if (!LineDrawingViewportZoom_GetCenterViewport(state, &viewport)) return false;

    view = SpaceAdapter_BuildViewContext(state);
    minScale = LineDrawingViewportZoom_MinScaleForSceneBounds(&state->layout.scene3d.bounds,
                                                              &view,
                                                              viewport.width,
                                                              viewport.height,
                                                              state->grid.gridSize);
    if (state->freeViewCamera.enabled) {
        CoreViewport3DCommand shared_command = {0};
        FreeViewCamera next_camera = state->freeViewCamera;
        Grid next_grid = state->grid;
        const double center_x = (double)viewport.x + (double)viewport.width * 0.5;
        const double center_y = (double)viewport.y + (double)viewport.height * 0.5;
        shared_command.kind = CORE_VIEWPORT3D_COMMAND_ZOOM;
        shared_command.value.zoom.factor = (double)zoomFactor;
        shared_command.value.zoom.anchor_offset_x = (double)anchorScreenX - center_x;
        shared_command.value.zoom.anchor_offset_y = (double)anchorScreenY - center_y;
        if (!LineDrawingViewport3DBridgeApply(&state->freeViewCamera,
                                              &state->grid,
                                              center_x,
                                              center_y,
                                              (double)minScale,
                                              (double)GRID_DEFAULT_MAX_SCALE,
                                              &shared_command,
                                              &next_camera,
                                              &next_grid)) return false;
        if (next_grid.scale == state->grid.scale &&
            next_camera.target.x == state->freeViewCamera.target.x &&
            next_camera.target.y == state->freeViewCamera.target.y &&
            next_camera.target.z == state->freeViewCamera.target.z) return false;
        state->freeViewCamera = next_camera;
        state->grid = next_grid;
        return true;
    }
    if (!LineDrawingViewportNavState_FromRuntime(&state->freeViewCamera,
                                                 &state->grid,
                                                 &before)) {
        return false;
    }
    command = (LineDrawingViewportNavCommand){
        .kind = LINE_DRAWING_VIEWPORT_NAV_COMMAND_ZOOM,
        .zoom_factor = zoomFactor,
        .anchor_screen_x = anchorScreenX,
        .anchor_screen_y = anchorScreenY,
        .grid_size = state->grid.gridSize,
        .min_zoom_scale = minScale,
        .max_zoom_scale = GRID_DEFAULT_MAX_SCALE
    };
    if (!LineDrawingViewportNavApply(&before, &command, &after)) return false;
    if (after.zoom_scale == before.zoom_scale &&
        after.view_offset_x == before.view_offset_x &&
        after.view_offset_y == before.view_offset_y) {
        return false;
    }
    return LineDrawingViewportNavState_Commit(&after,
                                              &state->freeViewCamera,
                                              &state->grid);
}
