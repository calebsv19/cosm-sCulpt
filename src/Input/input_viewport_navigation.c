#include "Input/input_viewport_navigation.h"

#include "Core/global_state.h"
#include "Core/line_drawing_pane_host.h"
#include "Core/space_mode_adapter.h"
#include "Core/viewport3d_bridge.h"
#include "Core/viewport_navigation_contract.h"
#include "Input/input_mouse_drag_shared.h"
#include "Input/input_mouse_internal.h"
#include "UI/ui_panel.h"
#include "UI/workspace_authoring/line_drawing_workspace_authoring_host.h"

static bool s_middle_pan_active = false;

static bool input_viewport_navigation_drag_conflict_active(void) {
    return draggingAnchor || draggingHandle || draggingSelectionBox || draggingGizmo ||
           draggingObjectResize || draggingObjectGizmo ||
           draggingObjectTranslate || draggingObjectRotate || draggingObjectScale ||
           draggingSceneBoundsGizmo || draggingSceneAuthoringPathHandle;
}

static bool input_viewport_navigation_modal_active(const GlobalState* state) {
    return !state ||
           LineDrawingWorkspaceAuthoringHost_Active(state) ||
           InputMouse_IsObjectFaceAuthoringModal(&state->editor) ||
           UIPanel_IsCapturingKeyboard() ||
           UIPanel_IsSaveDialogActive() ||
           UIPanel_IsRootDialogActive() ||
           UIPanel_IsPrismDimensionDialogActive() ||
           UIPanel_IsSceneBoundsDialogActive() ||
           UIPanel_IsConstructionPlaneDialogActive() ||
           UIPanel_IsObjectTransformDialogActive();
}

static bool input_viewport_navigation_apply_free_view_command(
    GlobalState* state,
    const LineDrawingViewportNavCommand* command) {
    CorePaneRect viewport = {0};
    CoreViewport3DCommand shared_command = {0};
    FreeViewCamera next_camera;
    Grid next_grid;
    const double degrees_to_radians = 3.14159265358979323846 / 180.0;
    if (!state || !command) return false;
    if (!LineDrawingPaneHost_GetViewportRect(&state->paneHost, &viewport)) {
        viewport = (CorePaneRect){0.0f, 0.0f,
                                  (float)state->screenWidth,
                                  (float)state->screenHeight};
    }
    if (command->kind == LINE_DRAWING_VIEWPORT_NAV_COMMAND_PAN) {
        shared_command.kind = CORE_VIEWPORT3D_COMMAND_PAN;
        shared_command.value.pan.screen_dx = (double)command->screen_dx;
        shared_command.value.pan.screen_dy = (double)command->screen_dy;
    } else if (command->kind == LINE_DRAWING_VIEWPORT_NAV_COMMAND_ORBIT) {
        shared_command.kind = CORE_VIEWPORT3D_COMMAND_ORBIT;
        shared_command.value.orbit.azimuth_delta_rad =
            (double)command->screen_dx * (double)command->orbit_yaw_per_pixel *
            degrees_to_radians;
        shared_command.value.orbit.elevation_delta_rad =
            (double)command->screen_dy * (double)command->orbit_pitch_per_pixel *
            degrees_to_radians;
    } else {
        return false;
    }
    next_camera = state->freeViewCamera;
    next_grid = state->grid;
    if (!LineDrawingViewport3DBridgeApply(
            &state->freeViewCamera,
            &state->grid,
            (double)viewport.x + (double)viewport.width * 0.5,
            (double)viewport.y + (double)viewport.height * 0.5,
            0.01,
            (double)GRID_DEFAULT_MAX_SCALE,
            &shared_command,
            &next_camera,
            &next_grid)) return false;
    state->freeViewCamera = next_camera;
    state->grid = next_grid;
    Global_FlagHitboxesDirty();
    return true;
}

bool InputViewportNavigation_HandleMouseButton(const SDL_MouseButtonEvent* button) {
    GlobalState* state = Global_Get();
    if (!button || button->button != SDL_BUTTON_MIDDLE) return false;
    if (button->type == SDL_MOUSEBUTTONUP) {
        const bool consumed = s_middle_pan_active;
        s_middle_pan_active = false;
        return consumed;
    }
    if (button->type != SDL_MOUSEBUTTONDOWN ||
        input_viewport_navigation_modal_active(state) ||
        input_viewport_navigation_drag_conflict_active() ||
        ResolvePointerPaneLane(button->x, button->y) != POINTER_PANE_CENTER) {
        return false;
    }
    s_middle_pan_active = true;
    return true;
}

bool InputViewportNavigation_HandleMouseMotion(const SDL_MouseMotionEvent* motion) {
    GlobalState* state = Global_Get();
    SpaceViewContext view_ctx = {0};
    SDL_Keymod mods = KMOD_NONE;
    if (!motion || !state) return false;
    if (input_viewport_navigation_modal_active(state) ||
        input_viewport_navigation_drag_conflict_active() ||
        ResolvePointerPaneLane(motion->x, motion->y) != POINTER_PANE_CENTER) {
        return false;
    }
    view_ctx = SpaceAdapter_BuildViewContext(state);
    if (s_middle_pan_active) {
        if ((motion->state & SDL_BUTTON_MMASK) == 0) {
            s_middle_pan_active = false;
            return false;
        }
        if (SpaceAdapter_IsFreeViewEnabled(&view_ctx)) {
            const LineDrawingViewportNavCommand command = {
                .kind = LINE_DRAWING_VIEWPORT_NAV_COMMAND_PAN,
                .screen_dx = (float)motion->xrel,
                .screen_dy = (float)motion->yrel,
                .grid_size = state->grid.gridSize
            };
            return input_viewport_navigation_apply_free_view_command(state, &command);
        }
        Grid_pan(&state->grid, -(float)motion->xrel, -(float)motion->yrel);
        Global_FlagGridChanged();
        return true;
    }

    mods = SDL_GetModState();
    if (!SpaceAdapter_IsFreeViewEnabled(&view_ctx) ||
        (mods & KMOD_ALT) == 0 ||
        (motion->state & SDL_BUTTON_LMASK) == 0) {
        return false;
    }
    {
        const LineDrawingViewportNavCommand command = {
            .kind = LINE_DRAWING_VIEWPORT_NAV_COMMAND_ORBIT,
            .screen_dx = (float)motion->xrel,
            .screen_dy = (float)motion->yrel,
            .orbit_yaw_per_pixel = 0.35f,
            .orbit_pitch_per_pixel = -0.35f
        };
        return input_viewport_navigation_apply_free_view_command(state, &command);
    }
}

void InputViewportNavigation_ResetGesture(void) {
    s_middle_pan_active = false;
}
