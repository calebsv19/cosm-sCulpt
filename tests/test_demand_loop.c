#include "test_framework.h"
#include "Core/SDLApp/sdl_app_loop_policy.h"

static bool demand_wait_preserves_input_and_timers(void) {
    TEST_ASSERT(SDLAppLoop_DemandWaitMs(true, false, false, -1) == 0);
    TEST_ASSERT(SDLAppLoop_DemandWaitMs(false, false, false, -1) == 1000);
    TEST_ASSERT(SDLAppLoop_DemandWaitMs(false, false, false, 16) == 16);
    TEST_ASSERT(SDLAppLoop_DemandWaitMs(false, false, false, 0) == 0);
    TEST_ASSERT(SDLAppLoop_DemandWaitMs(true, true, false, 0) == 1000);
    TEST_ASSERT(SDLAppLoop_DemandWaitMs(true, false, true, 0) == 50);
    TEST_ASSERT(SDLAppLoop_DemandWaitMs(false, false, false, 1400) == 1000);
    return true;
}
bool demand_loop_run_tests(void) {
    const TestCase cases[] = {{"input_timer_suspend_retry", demand_wait_preserves_input_and_timers}};
    return run_test_cases("DemandLoop", cases, sizeof(cases) / sizeof(cases[0]));
}
