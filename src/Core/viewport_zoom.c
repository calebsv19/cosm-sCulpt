#include "Core/viewport_zoom.h"

#include "Core/global_state.h"
#include "Core/line_drawing_pane_host.h"

#include <float.h>
#include <math.h>

enum {
    LINE_DRAWING_ZOOM_FIT_PADDING_PERCENT = 84
};

static const float LINE_DRAWING_ZOOM_ABSOLUTE_MIN_SCALE = 0.01f;
static const float LINE_DRAWING_ZOOM_EXTENT_EPSILON = 0.0001f;

static bool LineDrawingViewportZoom_GetCenterViewport(const GlobalState* state,
                                                      float* out_width,
                                                      float* out_height) {
    CorePaneRect center = {0};

    if (!state || !out_width || !out_height) return false;
    if (state->paneHost.initialized &&
        LineDrawingPaneHost_GetRectForRole(&state->paneHost,
                                           LINE_DRAWING_PANE_ROLE_CENTER_CANVAS,
                                           &center) &&
        center.width > 1.0f &&
        center.height > 1.0f) {
        *out_width = center.width;
        *out_height = center.height;
        return true;
    }

    *out_width = (float)state->screenWidth;
    *out_height = (float)state->screenHeight;
    return *out_width > 1.0f && *out_height > 1.0f;
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
    float viewportW = 0.0f;
    float viewportH = 0.0f;
    float minScale = GRID_DEFAULT_MIN_SCALE;
    float beforeScale = 0.0f;
    float beforeOffsetX = 0.0f;
    float beforeOffsetY = 0.0f;

    if (!state) return false;
    if (!LineDrawingViewportZoom_GetCenterViewport(state, &viewportW, &viewportH)) return false;

    view = SpaceAdapter_BuildViewContext(state);
    minScale = LineDrawingViewportZoom_MinScaleForSceneBounds(&state->layout.scene3d.bounds,
                                                              &view,
                                                              viewportW,
                                                              viewportH,
                                                              state->grid.gridSize);
    beforeScale = state->grid.scale;
    beforeOffsetX = state->grid.offsetX;
    beforeOffsetY = state->grid.offsetY;
    Grid_zoom_clamped(&state->grid,
                      zoomFactor,
                      anchorScreenX,
                      anchorScreenY,
                      minScale,
                      GRID_DEFAULT_MAX_SCALE);

    return state->grid.scale != beforeScale ||
           state->grid.offsetX != beforeOffsetX ||
           state->grid.offsetY != beforeOffsetY;
}
