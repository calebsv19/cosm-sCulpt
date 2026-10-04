#include "Core/viewport_zoom.h"

#include "Core/global_state.h"
#include "Layout/layout_engineering.h"
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

/* Keep engineering zoom meaningful when grid cells are fractions of a meter.
 * The legacy cap remains a floor; physical display density is independent of cells. */
static float LineDrawingViewportZoom_MaxScale(const GlobalState* state) {
    double size = state->grid.gridSize, meters = Layout_WorldScale(&state->layout);
    if (!isfinite(size) || size <= 0 || !isfinite(meters) || meters <= 0) return GRID_DEFAULT_MAX_SCALE;
    return (float)fmin(FLT_MAX, fmax(GRID_DEFAULT_MAX_SCALE, 10000 * meters / size));
}

bool LineDrawingViewportZoom_FitVisibleGeometry(GlobalState* state) {
    CorePaneRect viewport;
    if (!state || !LineDrawingViewportZoom_GetCenterViewport(state, &viewport) ||
        !isfinite(state->grid.gridSize) || state->grid.gridSize <= 0)
        return false;
    bool found = false;
    Vec3 min = {0}, max = {0};
    for (size_t i = 0; i < state->layout.objectStore.count; ++i) {
        const Object3D* o = &state->layout.objectStore.items[i];
        Vec3 low, high;
        if (!Layout_ObjectShown(&state->layout.objectStore, o) ||
            !Layout_Object3D_ComputeWorldAABB(o, &low, &high))
            continue;
        if (!found) {
            min = low;
            max = high;
            found = true;
        } else {
            min = (Vec3){fminf(min.x, low.x), fminf(min.y, low.y), fminf(min.z, low.z)};
            max = (Vec3){fmaxf(max.x, high.x), fmaxf(max.y, high.y), fmaxf(max.z, high.z)};
        }
    }
    for (size_t i = 0; i < state->layout.anchorCount; ++i) {
        const Anchor* a = &state->layout.anchors[i];
        if (a->isDeleted)
            continue;
        if (!found) {
            min = max = a->pos;
            found = true;
        } else {
            min = (Vec3){fminf(min.x, a->pos.x), fminf(min.y, a->pos.y), fminf(min.z, a->pos.z)};
            max = (Vec3){fmaxf(max.x, a->pos.x), fmaxf(max.y, a->pos.y), fmaxf(max.z, a->pos.z)};
        }
    }
    if (!found)
        return false;
    SpaceViewContext view = SpaceAdapter_BuildViewContext(state);
    if (SpaceAdapter_IsFreeViewEnabled(&view))
        view.camera.target =
            (Vec3){min.x * .5f + max.x * .5f, min.y * .5f + max.y * .5f, min.z * .5f + max.z * .5f};
    double x0 = DBL_MAX, y0 = DBL_MAX, x1 = -DBL_MAX, y1 = -DBL_MAX;
    for (int i = 0; i < 8; ++i) {
        Vec2 p = SpaceAdapter_ProjectToView(
            (Vec3){i & 1 ? max.x : min.x, i & 2 ? max.y : min.y, i & 4 ? max.z : min.z}, &view);
        if (!isfinite(p.x) || !isfinite(p.y))
            return false;
        x0 = fmin(x0, p.x);
        x1 = fmax(x1, p.x);
        y0 = fmin(y0, p.y);
        y1 = fmax(y1, p.y);
    }
    double pixels = .84 * fmin(viewport.width / fmax(x1 - x0, .001), viewport.height / fmax(y1 - y0, .001));
    double scale = fmin(LineDrawingViewportZoom_MaxScale(state), fmax(.01, pixels / state->grid.gridSize));
    pixels = scale * state->grid.gridSize;
    double dx = (x0 + x1) * .5 - (viewport.x + viewport.width * .5) / pixels;
    double dy = (y0 + y1) * .5 - (viewport.y + viewport.height * .5) / pixels;
    if (!isfinite(scale) || !isfinite(dx) || !isfinite(dy) || fabs(dx) > FLT_MAX || fabs(dy) > FLT_MAX)
        return false;
    state->grid.scale = (float)scale;
    state->grid.offsetX = (float)dx;
    state->grid.offsetY = (float)dy;
    if (SpaceAdapter_IsFreeViewEnabled(&view))
        state->freeViewCamera.target = view.camera.target;
    Global_FlagGridChanged();
    Global_FlagHitboxesDirty();
    return true;
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
                                              (double)LineDrawingViewportZoom_MaxScale(state),
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
        .max_zoom_scale = LineDrawingViewportZoom_MaxScale(state)
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
