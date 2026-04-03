#include "test_framework.h"

#include "Input/input_routing_policy.h"

#include <string.h>

static SDL_Event key_event(Uint32 type, SDL_Keycode keycode) {
    SDL_Event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.key.type = type;
    event.key.keysym.sym = keycode;
    return event;
}

static bool test_escape_is_global_when_not_capturing(void) {
    SDL_Event event = key_event(SDL_KEYDOWN, SDLK_ESCAPE);
    LineDrawingInputPolicyDecision decision = LineDrawingInputPolicyNormalize(&event, false);
    TEST_ASSERT(decision.action_class == LINE_DRAWING_INPUT_ACTION_IMMEDIATE);
    TEST_ASSERT(decision.route_policy == LINE_DRAWING_INPUT_ROUTE_GLOBAL);
    TEST_ASSERT(decision.global_shortcuts_allowed);
    TEST_ASSERT(!decision.global_shortcut_blocked);
    TEST_ASSERT(decision.request_quit);
    return true;
}

static bool test_escape_blocked_when_text_capture_active(void) {
    SDL_Event event = key_event(SDL_KEYDOWN, SDLK_ESCAPE);
    LineDrawingInputPolicyDecision decision = LineDrawingInputPolicyNormalize(&event, true);
    TEST_ASSERT(decision.action_class == LINE_DRAWING_INPUT_ACTION_IMMEDIATE);
    TEST_ASSERT(decision.route_policy == LINE_DRAWING_INPUT_ROUTE_FOCUSED);
    TEST_ASSERT(!decision.global_shortcuts_allowed);
    TEST_ASSERT(decision.global_shortcut_blocked);
    TEST_ASSERT(!decision.request_quit);
    return true;
}

static bool test_quit_event_always_routes_global(void) {
    SDL_Event event;
    memset(&event, 0, sizeof(event));
    event.type = SDL_QUIT;

    LineDrawingInputPolicyDecision decisionA = LineDrawingInputPolicyNormalize(&event, false);
    LineDrawingInputPolicyDecision decisionB = LineDrawingInputPolicyNormalize(&event, true);
    TEST_ASSERT(decisionA.route_policy == LINE_DRAWING_INPUT_ROUTE_GLOBAL);
    TEST_ASSERT(decisionA.request_quit);
    TEST_ASSERT(decisionB.route_policy == LINE_DRAWING_INPUT_ROUTE_GLOBAL);
    TEST_ASSERT(decisionB.request_quit);
    return true;
}

static bool test_regular_key_routes_focused_without_capture(void) {
    SDL_Event event = key_event(SDL_KEYDOWN, SDLK_c);
    LineDrawingInputPolicyDecision decision = LineDrawingInputPolicyNormalize(&event, false);
    TEST_ASSERT(decision.action_class == LINE_DRAWING_INPUT_ACTION_IMMEDIATE);
    TEST_ASSERT(decision.route_policy == LINE_DRAWING_INPUT_ROUTE_FOCUSED);
    TEST_ASSERT(decision.global_shortcuts_allowed);
    TEST_ASSERT(!decision.global_shortcut_blocked);
    TEST_ASSERT(!decision.request_quit);
    return true;
}

bool input_policy_run_tests(void) {
    const TestCase cases[] = {
        { "EscapeGlobalNoCapture", test_escape_is_global_when_not_capturing },
        { "EscapeBlockedWithCapture", test_escape_blocked_when_text_capture_active },
        { "QuitAlwaysGlobal", test_quit_event_always_routes_global },
        { "RegularKeyFocused", test_regular_key_routes_focused_without_capture },
    };
    return run_test_cases("InputPolicy", cases, sizeof(cases) / sizeof(cases[0]));
}
