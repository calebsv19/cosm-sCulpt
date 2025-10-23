#include "test_framework.h"

#include "Layout/Grid/grid.h"
#include "Math/math_util.h"

#include <math.h>

static bool test_vec2_snap_aligns_to_grid(void) {
    Vec2 input = { 0.76f, -1.21f };
    Vec2 snapped = Vec2_Snap(input, 0.5f);

    TEST_ASSERT(fabsf(fmodf(snapped.x, 0.5f)) < 1e-6f);
    TEST_ASSERT(fabsf(fmodf(snapped.y, 0.5f)) < 1e-6f);
    return true;
}

static bool test_screen_to_world_roundtrip(void) {
    Grid grid;
    Grid_init(&grid, 1.0f, 800, 600);

    int sx = 420;
    int sy = 315;

    Vec2 world = ScreenToWorld(sx, sy, &grid);
    Vec2 back = WorldToScreen(world, &grid);

    TEST_ASSERT(fabsf(back.x - (float)sx) < 0.01f);
    TEST_ASSERT(fabsf(back.y - (float)sy) < 0.01f);
    return true;
}

bool math_run_tests(void) {
    const TestCase cases[] = {
        { "Vec2SnapAlignsToGrid", test_vec2_snap_aligns_to_grid },
        { "ScreenWorldRoundtrip", test_screen_to_world_roundtrip }
    };

    return run_test_cases("Math", cases, sizeof(cases) / sizeof(cases[0]));
}
