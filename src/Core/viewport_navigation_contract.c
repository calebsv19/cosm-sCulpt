#include "Core/viewport_navigation_contract.h"

#include <math.h>

static bool line_drawing_viewport_nav_vec3_finite(Vec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static bool line_drawing_viewport_nav_state_valid(const LineDrawingViewportNavState* state) {
    return state &&
           line_drawing_viewport_nav_vec3_finite(state->target) &&
           isfinite(state->yaw_deg) &&
           isfinite(state->pitch_deg) &&
           isfinite(state->zoom_scale) && state->zoom_scale > 0.0f &&
           isfinite(state->view_offset_x) &&
           isfinite(state->view_offset_y);
}

static float line_drawing_viewport_nav_normalize_degrees(float value) {
    float normalized = fmodf(value, 360.0f);
    if (normalized < 0.0f) normalized += 360.0f;
    return normalized;
}

static float line_drawing_viewport_nav_normalize_signed_degrees(float value) {
    float normalized = line_drawing_viewport_nav_normalize_degrees(value);
    if (normalized > 180.0f) normalized -= 360.0f;
    return normalized;
}

bool LineDrawingViewportNavState_FromRuntime(const FreeViewCamera* camera,
                                             const Grid* grid,
                                             LineDrawingViewportNavState* out_state) {
    LineDrawingViewportNavState candidate = {0};
    if (!camera || !grid || !out_state ||
        !isfinite(grid->gridSize) || grid->gridSize <= 0.0f) {
        return false;
    }
    candidate.target = camera->target;
    candidate.yaw_deg = camera->yawDeg;
    candidate.pitch_deg = camera->pitchDeg;
    candidate.zoom_scale = grid->scale;
    candidate.view_offset_x = grid->offsetX;
    candidate.view_offset_y = grid->offsetY;
    if (!line_drawing_viewport_nav_state_valid(&candidate)) return false;
    *out_state = candidate;
    return true;
}

bool LineDrawingViewportNavState_Commit(const LineDrawingViewportNavState* nav_state,
                                        FreeViewCamera* camera,
                                        Grid* grid) {
    if (!line_drawing_viewport_nav_state_valid(nav_state) || !camera || !grid) return false;
    camera->target = nav_state->target;
    camera->yawDeg = nav_state->yaw_deg;
    camera->pitchDeg = nav_state->pitch_deg;
    grid->scale = nav_state->zoom_scale;
    grid->offsetX = nav_state->view_offset_x;
    grid->offsetY = nav_state->view_offset_y;
    return true;
}

bool LineDrawingViewportNavApply(const LineDrawingViewportNavState* state,
                                 const LineDrawingViewportNavCommand* command,
                                 LineDrawingViewportNavState* out_state) {
    LineDrawingViewportNavState candidate = {0};
    FreeViewCamera camera = {0};
    if (!line_drawing_viewport_nav_state_valid(state) || !command || !out_state) return false;
    candidate = *state;
    camera.enabled = true;
    camera.target = state->target;
    camera.yawDeg = state->yaw_deg;
    camera.pitchDeg = state->pitch_deg;

    switch (command->kind) {
        case LINE_DRAWING_VIEWPORT_NAV_COMMAND_ORBIT:
            if (!isfinite(command->screen_dx) ||
                !isfinite(command->screen_dy) ||
                !isfinite(command->orbit_yaw_per_pixel) ||
                !isfinite(command->orbit_pitch_per_pixel)) {
                return false;
            }
            candidate.yaw_deg = line_drawing_viewport_nav_normalize_degrees(
                candidate.yaw_deg + command->screen_dx * command->orbit_yaw_per_pixel);
            candidate.pitch_deg = line_drawing_viewport_nav_normalize_signed_degrees(
                candidate.pitch_deg + command->screen_dy * command->orbit_pitch_per_pixel);
            break;

        case LINE_DRAWING_VIEWPORT_NAV_COMMAND_PAN: {
            Vec3 right = {0};
            Vec3 up = {0};
            float pixels_per_unit = 0.0f;
            if (!isfinite(command->screen_dx) || !isfinite(command->screen_dy) ||
                !isfinite(command->grid_size) || command->grid_size <= 0.0f) {
                return false;
            }
            pixels_per_unit = command->grid_size * candidate.zoom_scale;
            if (!isfinite(pixels_per_unit) || pixels_per_unit <= 0.000001f) return false;
            right = FreeView_Right(&camera);
            up = FreeView_Up(&camera);
            candidate.target = Vec3_Sub(candidate.target,
                                        Vec3_Add(Vec3_Scale(right,
                                                           command->screen_dx / pixels_per_unit),
                                                 Vec3_Scale(up,
                                                           command->screen_dy / pixels_per_unit)));
            break;
        }

        case LINE_DRAWING_VIEWPORT_NAV_COMMAND_ZOOM: {
            float new_scale = 0.0f;
            float old_pixels_per_unit = 0.0f;
            float new_pixels_per_unit = 0.0f;
            float anchor_world_x = 0.0f;
            float anchor_world_y = 0.0f;
            if (!isfinite(command->zoom_factor) || command->zoom_factor <= 0.0f ||
                !isfinite(command->anchor_screen_x) || !isfinite(command->anchor_screen_y) ||
                !isfinite(command->grid_size) || command->grid_size <= 0.0f ||
                !isfinite(command->min_zoom_scale) || command->min_zoom_scale <= 0.0f ||
                !isfinite(command->max_zoom_scale) ||
                command->max_zoom_scale < command->min_zoom_scale) {
                return false;
            }
            new_scale = candidate.zoom_scale * command->zoom_factor;
            if (new_scale < command->min_zoom_scale) new_scale = command->min_zoom_scale;
            if (new_scale > command->max_zoom_scale) new_scale = command->max_zoom_scale;
            old_pixels_per_unit = candidate.zoom_scale * command->grid_size;
            new_pixels_per_unit = new_scale * command->grid_size;
            if (old_pixels_per_unit <= 0.0f || new_pixels_per_unit <= 0.0f) return false;
            anchor_world_x = command->anchor_screen_x / old_pixels_per_unit +
                             candidate.view_offset_x;
            anchor_world_y = command->anchor_screen_y / old_pixels_per_unit +
                             candidate.view_offset_y;
            candidate.zoom_scale = new_scale;
            candidate.view_offset_x = anchor_world_x -
                                      command->anchor_screen_x / new_pixels_per_unit;
            candidate.view_offset_y = anchor_world_y -
                                      command->anchor_screen_y / new_pixels_per_unit;
            break;
        }

        case LINE_DRAWING_VIEWPORT_NAV_COMMAND_FRAME: {
            float pixels_per_unit = 0.0f;
            if (!line_drawing_viewport_nav_vec3_finite(command->frame_target) ||
                !isfinite(command->frame_zoom_scale) || command->frame_zoom_scale <= 0.0f ||
                !isfinite(command->viewport_center_x) ||
                !isfinite(command->viewport_center_y) ||
                !isfinite(command->grid_size) || command->grid_size <= 0.0f) {
                return false;
            }
            pixels_per_unit = command->frame_zoom_scale * command->grid_size;
            candidate.target = command->frame_target;
            candidate.zoom_scale = command->frame_zoom_scale;
            candidate.view_offset_x = -command->viewport_center_x / pixels_per_unit;
            candidate.view_offset_y = -command->viewport_center_y / pixels_per_unit;
            break;
        }

        case LINE_DRAWING_VIEWPORT_NAV_COMMAND_RESET:
            if (!line_drawing_viewport_nav_vec3_finite(command->reset_target) ||
                !isfinite(command->reset_yaw_deg) ||
                !isfinite(command->reset_pitch_deg) ||
                !isfinite(command->reset_zoom_scale) || command->reset_zoom_scale <= 0.0f ||
                !isfinite(command->viewport_center_x) ||
                !isfinite(command->viewport_center_y) ||
                !isfinite(command->grid_size) || command->grid_size <= 0.0f) {
                return false;
            }
            candidate.target = command->reset_target;
            candidate.yaw_deg = line_drawing_viewport_nav_normalize_degrees(command->reset_yaw_deg);
            candidate.pitch_deg = line_drawing_viewport_nav_normalize_signed_degrees(
                command->reset_pitch_deg);
            candidate.zoom_scale = command->reset_zoom_scale;
            candidate.view_offset_x = -command->viewport_center_x /
                                      (command->grid_size * command->reset_zoom_scale);
            candidate.view_offset_y = -command->viewport_center_y /
                                      (command->grid_size * command->reset_zoom_scale);
            break;

        case LINE_DRAWING_VIEWPORT_NAV_COMMAND_RESIZE:
            break;

        case LINE_DRAWING_VIEWPORT_NAV_COMMAND_NONE:
        default:
            return false;
    }

    if (!line_drawing_viewport_nav_state_valid(&candidate)) return false;
    *out_state = candidate;
    return true;
}
