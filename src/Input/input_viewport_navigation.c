#include "Input/input_viewport_navigation.h"

#include "Core/global_state.h"
#include "Core/line_drawing_pane_host.h"
#include "Core/space_mode_adapter.h"
#include "Core/viewport3d_bridge.h"
#include "Core/viewport_zoom.h"
#include "Core/viewport_navigation_contract.h"
#include "Input/input_mouse_drag_shared.h"
#include "Input/input_mouse_internal.h"
#include "UI/ui_panel.h"
#include "UI/workspace_authoring/line_drawing_workspace_authoring_host.h"

static Uint8 s_gesture_button = 0;
static bool s_orbit_active = false;

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
            (double)LineDrawingViewportZoom_MaxScale(state),
            &shared_command,
            &next_camera,
            &next_grid)) return false;
    state->freeViewCamera = next_camera;
    state->grid = next_grid;
    Global_FlagHitboxesDirty();
    return true;
}

/* Capture camera intent at press time, before hit-testing geometry or handles.
 * A gesture belongs to its starting viewport until release, even outside it. */
bool InputViewportNavigation_HandleMouseButton(const SDL_MouseButtonEvent* button) {
    GlobalState* state = Global_Get();
    if (!button) return false;
    if (button->type == SDL_MOUSEBUTTONUP) {
        if (s_gesture_button != button->button) return false;
        InputViewportNavigation_ResetGesture();
        return true;
    }
    if (button->type != SDL_MOUSEBUTTONDOWN ||
        input_viewport_navigation_modal_active(state) ||
        input_viewport_navigation_drag_conflict_active() ||
        ResolvePointerPaneLane(button->x, button->y) != POINTER_PANE_CENTER)
        return false;
    const bool option = (SDL_GetModState() & KMOD_ALT) != 0;
    const ViewportTool tool = state->editor.viewportTool;
    const bool orbit = button->button == SDL_BUTTON_LEFT &&
                       (option || tool == VIEWPORT_TOOL_ORBIT);
    const bool pan = button->button == SDL_BUTTON_MIDDLE ||
                     (button->button == SDL_BUTTON_RIGHT && tool != VIEWPORT_TOOL_LINE &&
                      !InputMouse_IsObjectFaceAuthoringModal(&state->editor) &&
                      !state->editor.objectFaceSketchHasRectangle &&
                      !state->editor.objectFaceExtrudeHasPreview) ||
                     (button->button == SDL_BUTTON_LEFT && tool == VIEWPORT_TOOL_PAN);
    if (!orbit && !pan) return false;
    if (orbit) {
        if (state->spaceMode != SPACE_MODE_3D) return false;
        /* Option-drag works from a named view too; preserve framing and geometry. */
        state->freeViewCamera.enabled = true;
    }
    s_gesture_button = button->button;
    s_orbit_active = orbit;
    return true;
}

bool InputViewportNavigation_HandleMouseMotion(const SDL_MouseMotionEvent* motion) {
    GlobalState* state = Global_Get();
    if (!motion || !state || !s_gesture_button) return false;
    if ((motion->state & SDL_BUTTON(s_gesture_button)) == 0) {
        InputViewportNavigation_ResetGesture();
        return false;
    }
    if (input_viewport_navigation_modal_active(state)) {
        InputViewportNavigation_ResetGesture();
        return true;
    }
    SpaceViewContext view = SpaceAdapter_BuildViewContext(state);
    if (SpaceAdapter_IsFreeViewEnabled(&view)) {
        const LineDrawingViewportNavCommand command = {
            .kind = s_orbit_active ? LINE_DRAWING_VIEWPORT_NAV_COMMAND_ORBIT :
                                    LINE_DRAWING_VIEWPORT_NAV_COMMAND_PAN,
            .screen_dx = (float)motion->xrel,
            .screen_dy = (float)motion->yrel,
            .grid_size = state->grid.gridSize,
            .orbit_yaw_per_pixel = 0.35f,
            .orbit_pitch_per_pixel = -0.35f
        };
        (void)input_viewport_navigation_apply_free_view_command(state, &command);
    } else {
        Grid_pan(&state->grid, -(float)motion->xrel, -(float)motion->yrel);
        Global_FlagGridChanged();
    }
    /* A failed camera update must never fall through to a geometry drag. */
    return true;
}

void InputViewportNavigation_ResetGesture(void) {
    s_gesture_button = 0;
    s_orbit_active = false;
}
