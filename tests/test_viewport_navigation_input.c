#include "test_layout_internal.h"

#include "Core/line_drawing_pane_host.h"
#include "Input/input_mouse.h"
#include "Input/input_viewport_navigation.h"
#include "Core/viewport_zoom.h"
#include "UI/input_ui_panel.h"
#include "Layout/layout_engineering.h"

#include <SDL2/SDL.h>

static bool test_alt_lmb_orbit_requires_button_and_preserves_target(void) {
    GlobalState* state = NULL;
    CorePaneRect viewport = {0};
    SDL_Event event = {0};
    SDL_Keymod saved_mods = SDL_GetModState();
    Vec3 target_before = {0};
    float yaw_before = 0.0f;
    int cx = 400;
    int cy = 300;

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.target = (Vec3){ 7.0f, 8.0f, 9.0f };
    target_before = state->freeViewCamera.target;
    yaw_before = state->freeViewCamera.yawDeg;
    if (LineDrawingPaneHost_GetViewportRect(&state->paneHost, &viewport)) {
        cx = (int)(viewport.x + viewport.width * 0.5f);
        cy = (int)(viewport.y + viewport.height * 0.5f);
    }

    SDL_SetModState(KMOD_ALT);
    event.type = SDL_MOUSEMOTION;
    event.motion.type = SDL_MOUSEMOTION;
    event.motion.x = cx;
    event.motion.y = cy;
    event.motion.xrel = 20;
    event.motion.yrel = 10;
    event.motion.state = 0;
    Input_MouseHandle(NULL, &event);
    TEST_ASSERT(ld_test_nearly_equal(state->freeViewCamera.yawDeg, yaw_before));

    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = cx;
    event.button.y = cy;
    event.button.clicks = 1;
    Input_MouseHandle(NULL, &event);

    event.type = SDL_MOUSEMOTION;
    event.motion.type = SDL_MOUSEMOTION;
    event.motion.x = cx + 20;
    event.motion.y = cy + 10;
    event.motion.xrel = 20;
    event.motion.yrel = 10;
    event.motion.state = SDL_BUTTON_LMASK;
    Input_MouseHandle(NULL, &event);
    TEST_ASSERT(!ld_test_nearly_equal(state->freeViewCamera.yawDeg, yaw_before));
    TEST_ASSERT(ld_test_vec3_nearly_equal(state->freeViewCamera.target, target_before));

    event.type = SDL_MOUSEBUTTONUP;
    event.button.type = SDL_MOUSEBUTTONUP;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = cx + 20;
    event.button.y = cy + 10;
    Input_MouseHandle(NULL, &event);
    SDL_SetModState(saved_mods);
    InputViewportNavigation_ResetGesture();
    ld_test_shutdown_runtime();
    return true;
}

static bool test_middle_drag_pans_free_view_target_and_release_stops(void) {
    GlobalState* state = NULL;
    CorePaneRect viewport = {0};
    SDL_Event event = {0};
    Vec3 target_before = {0};
    Vec3 target_after = {0};
    int cx = 400;
    int cy = 300;

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.target = (Vec3){ 3.0f, 4.0f, 5.0f };
    target_before = state->freeViewCamera.target;
    if (LineDrawingPaneHost_GetViewportRect(&state->paneHost, &viewport)) {
        cx = (int)(viewport.x + viewport.width * 0.5f);
        cy = (int)(viewport.y + viewport.height * 0.5f);
    }

    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_MIDDLE;
    event.button.x = cx;
    event.button.y = cy;
    Input_MouseHandle(NULL, &event);

    event.type = SDL_MOUSEMOTION;
    event.motion.type = SDL_MOUSEMOTION;
    event.motion.x = cx + 24;
    event.motion.y = cy - 12;
    event.motion.xrel = 24;
    event.motion.yrel = -12;
    event.motion.state = SDL_BUTTON_MMASK;
    Input_MouseHandle(NULL, &event);
    TEST_ASSERT(!ld_test_vec3_nearly_equal(state->freeViewCamera.target, target_before));
    target_after = state->freeViewCamera.target;

    event.type = SDL_MOUSEBUTTONUP;
    event.button.type = SDL_MOUSEBUTTONUP;
    event.button.button = SDL_BUTTON_MIDDLE;
    event.button.x = cx + 24;
    event.button.y = cy - 12;
    Input_MouseHandle(NULL, &event);

    event.type = SDL_MOUSEMOTION;
    event.motion.type = SDL_MOUSEMOTION;
    event.motion.x = cx + 36;
    event.motion.y = cy;
    event.motion.xrel = 12;
    event.motion.yrel = 12;
    event.motion.state = 0;
    Input_MouseHandle(NULL, &event);
    TEST_ASSERT(ld_test_vec3_nearly_equal(state->freeViewCamera.target, target_after));

    InputViewportNavigation_ResetGesture();
    ld_test_shutdown_runtime();
    return true;
}

static bool test_fit_scene_button_physical_zoom_read_only(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Global_SetWindowSize(1400, 1000);
    UIPanel_OnWindowResized(1400, 1000);
    state->freeViewCamera.enabled = false;
    state->activePlane.axis = VIEW_PLANE_XY;
    state->grid.gridSize = .1f;
    state->layout.gridSize = .1f;
    RectPrismPrimitiveCreateParams p = {
        .width = 1.85f,
        .height = 3.65f,
        .depth = 1.9f,
        .useExplicitFrame = true,
        .explicitFrame = {
            .origin = {3, 4, .95f}, .axisU = {1, 0, 0}, .axisV = {0, 1, 0}, .normal = {0, 0, 1}}};
    uint32_t id;
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(&state->layout, &p, &id, NULL));
    Editor_ClearHistory(&state->editor);
    state->layoutDirtySinceSave = false;
    char* before = Layout_SaveToString(&state->layout);
    UIPanel_SetActiveRightTab(UIPanel_Get(), UI_PANEL_RIGHT_TAB_VIEW);
    UIPanel_OnWindowResized(1400, 1000);
    const UIButton* fit = NULL;
    for (int i = 0; i < UIPanel_Get()->count; ++i)
        if (UIPanel_Get()->buttons[i].id == UI_BTN_FIT_SCENE)
            fit = &UIPanel_Get()->buttons[i];
    TEST_ASSERT(fit && fit->bounds.w > 0 && fit->bounds.h > 0);
    SDL_Event e = {.type = SDL_MOUSEBUTTONDOWN};
    e.button.button = SDL_BUTTON_LEFT;
    e.button.x = fit->bounds.x + fit->bounds.w / 2;
    e.button.y = fit->bounds.y + fit->bounds.h / 2;
    TEST_ASSERT(UIPanel_HandleClick(e.button.x, e.button.y));
    CorePaneRect r;
    TEST_ASSERT(LineDrawingPaneHost_GetViewportRect(&state->paneHost, &r));
    SpaceViewContext view = SpaceAdapter_BuildViewContext(state);
    Vec3 low, high;
    TEST_ASSERT(Layout_Object3D_ComputeWorldAABB(&state->layout.objectStore.items[0], &low, &high));
    for (int i = 0; i < 8; ++i) {
        Vec2 screen = WorldToScreen(
            SpaceAdapter_ProjectToView(
                (Vec3){i & 1 ? high.x : low.x, i & 2 ? high.y : low.y, i & 4 ? high.z : low.z}, &view),
            &state->grid);
        TEST_ASSERT(screen.x > r.x && screen.x < r.x + r.width && screen.y > r.y &&
                    screen.y < r.y + r.height);
    }
    float fitted = state->grid.scale;
    TEST_ASSERT(fitted > 100);
    TEST_ASSERT(LineDrawingViewportZoom_Apply(state, 1.1f, r.x + r.width * .5f, r.y + r.height * .5f) &&
                state->grid.scale > fitted);
    char* after = Layout_SaveToString(&state->layout);
    TEST_ASSERT(!strcmp(before, after) && !state->layoutDirtySinceSave && !Editor_UndoCount(&state->editor));
    Layout_FreeString(before);
    Layout_FreeString(after);
    state->layout.objectStore.items[0].coreMeta.flags.visible = false;
    float scale = state->grid.scale;
    TEST_ASSERT(!LineDrawingViewportZoom_FitVisibleGeometry(state) && state->grid.scale == scale);
    state->layout.objectStore.items[0].coreMeta.flags.visible = true;
    state->freeViewCamera.enabled = true;
    TEST_ASSERT(LineDrawingViewportZoom_FitVisibleGeometry(state));
    TEST_ASSERT(fabs(state->freeViewCamera.target.x - 3) < 1e-5 &&
                fabs(state->freeViewCamera.target.y - 4) < 1e-5);
    ld_test_shutdown_runtime();
    return true;
}

bool viewport_navigation_input_run_tests(void) {
    const TestCase cases[] = {
        { "FitSceneButtonPhysicalZoomReadOnly",test_fit_scene_button_physical_zoom_read_only },
        { "AltLmbOrbitRequiresButtonAndPreservesTarget",
          test_alt_lmb_orbit_requires_button_and_preserves_target },
        { "MiddleDragPansFreeViewTargetAndReleaseStops",
          test_middle_drag_pans_free_view_target_and_release_stops }
    };
    return run_test_cases("ViewportNavigationInput",
                          cases,
                          sizeof(cases) / sizeof(cases[0]));
}
