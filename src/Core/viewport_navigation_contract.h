#pragma once

#include "Layout/Grid/grid.h"
#include "Math/math_util.h"

#include <stdbool.h>

typedef enum {
    LINE_DRAWING_VIEWPORT_NAV_COMMAND_NONE = 0,
    LINE_DRAWING_VIEWPORT_NAV_COMMAND_ORBIT,
    LINE_DRAWING_VIEWPORT_NAV_COMMAND_PAN,
    LINE_DRAWING_VIEWPORT_NAV_COMMAND_ZOOM,
    LINE_DRAWING_VIEWPORT_NAV_COMMAND_FRAME,
    LINE_DRAWING_VIEWPORT_NAV_COMMAND_RESET,
    LINE_DRAWING_VIEWPORT_NAV_COMMAND_RESIZE
} LineDrawingViewportNavCommandKind;

typedef struct {
    Vec3 target;
    float yaw_deg;
    float pitch_deg;
    float zoom_scale;
    float view_offset_x;
    float view_offset_y;
} LineDrawingViewportNavState;

typedef struct {
    LineDrawingViewportNavCommandKind kind;
    float screen_dx;
    float screen_dy;
    float orbit_yaw_per_pixel;
    float orbit_pitch_per_pixel;
    float zoom_factor;
    float anchor_screen_x;
    float anchor_screen_y;
    float grid_size;
    float min_zoom_scale;
    float max_zoom_scale;
    Vec3 frame_target;
    float frame_zoom_scale;
    float viewport_center_x;
    float viewport_center_y;
    Vec3 reset_target;
    float reset_yaw_deg;
    float reset_pitch_deg;
    float reset_zoom_scale;
} LineDrawingViewportNavCommand;

bool LineDrawingViewportNavState_FromRuntime(const FreeViewCamera* camera,
                                             const Grid* grid,
                                             LineDrawingViewportNavState* out_state);
bool LineDrawingViewportNavState_Commit(const LineDrawingViewportNavState* nav_state,
                                        FreeViewCamera* camera,
                                        Grid* grid);
bool LineDrawingViewportNavApply(const LineDrawingViewportNavState* state,
                                 const LineDrawingViewportNavCommand* command,
                                 LineDrawingViewportNavState* out_state);
