#include "test_layout_internal.h"

#include "Core/line_drawing_pane_host.h"
#include "Input/input_mouse.h"
#include "Input/input_viewport_navigation.h"

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

bool viewport_navigation_input_run_tests(void) {
    const TestCase cases[] = {
        { "AltLmbOrbitRequiresButtonAndPreservesTarget",
          test_alt_lmb_orbit_requires_button_and_preserves_target },
        { "MiddleDragPansFreeViewTargetAndReleaseStops",
          test_middle_drag_pans_free_view_target_and_release_stops }
    };
    return run_test_cases("ViewportNavigationInput",
                          cases,
                          sizeof(cases) / sizeof(cases[0]));
}
