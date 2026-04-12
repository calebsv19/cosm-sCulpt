#include "test_framework.h"

#include "Core/line_drawing_pane_host.h"

static bool test_pane_host_init_and_role_rects(void) {
    LineDrawingPaneHost host;
    CorePaneRect top = {0};
    CorePaneRect left = {0};
    CorePaneRect center = {0};
    CorePaneRect right = {0};

    TEST_ASSERT(LineDrawingPaneHost_Init(&host, 1280.0f, 720.0f));
    TEST_ASSERT(host.initialized);
    TEST_ASSERT(host.leaf_count == 4u);

    TEST_ASSERT(LineDrawingPaneHost_GetRectForRole(&host, LINE_DRAWING_PANE_ROLE_TOP_BAR, &top));
    TEST_ASSERT(LineDrawingPaneHost_GetRectForRole(&host, LINE_DRAWING_PANE_ROLE_LEFT_CONTROLS, &left));
    TEST_ASSERT(LineDrawingPaneHost_GetRectForRole(&host, LINE_DRAWING_PANE_ROLE_CENTER_CANVAS, &center));
    TEST_ASSERT(LineDrawingPaneHost_GetRectForRole(&host, LINE_DRAWING_PANE_ROLE_RIGHT_CONTROLS, &right));

    TEST_ASSERT(top.width > 1200.0f);
    TEST_ASSERT(top.height >= 52.0f);
    TEST_ASSERT(left.width >= 150.0f);
    TEST_ASSERT(left.height > 500.0f);
    TEST_ASSERT(center.width > right.width);
    TEST_ASSERT(center.height > 500.0f);
    return true;
}

static bool test_pane_host_rebuild_tracks_window_bounds(void) {
    LineDrawingPaneHost host;
    CorePaneRect center_before = {0};
    CorePaneRect center_after = {0};

    TEST_ASSERT(LineDrawingPaneHost_Init(&host, 1280.0f, 720.0f));
    TEST_ASSERT(LineDrawingPaneHost_GetRectForRole(&host,
                                                   LINE_DRAWING_PANE_ROLE_CENTER_CANVAS,
                                                   &center_before));
    TEST_ASSERT(LineDrawingPaneHost_Rebuild(&host, 1680.0f, 980.0f));
    TEST_ASSERT(LineDrawingPaneHost_GetRectForRole(&host,
                                                   LINE_DRAWING_PANE_ROLE_CENTER_CANVAS,
                                                   &center_after));

    TEST_ASSERT(host.bounds_width == 1680.0f);
    TEST_ASSERT(host.bounds_height == 980.0f);
    TEST_ASSERT(center_after.width > center_before.width);
    TEST_ASSERT(center_after.height > center_before.height);
    return true;
}

static bool test_pane_host_chrome_targets_allow_compact_side_panes(void) {
    LineDrawingPaneHost host;
    CorePaneRect left = {0};
    CorePaneRect right = {0};

    TEST_ASSERT(LineDrawingPaneHost_Init(&host, 1280.0f, 720.0f));
    LineDrawingPaneHost_SetChromeTargets(&host, 56.0f, 118.0f, 122.0f);
    TEST_ASSERT(LineDrawingPaneHost_Rebuild(&host, 1280.0f, 720.0f));
    TEST_ASSERT(LineDrawingPaneHost_GetRectForRole(&host, LINE_DRAWING_PANE_ROLE_LEFT_CONTROLS, &left));
    TEST_ASSERT(LineDrawingPaneHost_GetRectForRole(&host, LINE_DRAWING_PANE_ROLE_RIGHT_CONTROLS, &right));

    TEST_ASSERT(left.width >= 100.0f);
    TEST_ASSERT(left.width < 146.0f);
    TEST_ASSERT(right.width >= 104.0f);
    TEST_ASSERT(right.width < 150.0f);
    return true;
}

bool pane_host_run_tests(void) {
    const TestCase cases[] = {
        {"pane host init + role rects", test_pane_host_init_and_role_rects},
        {"pane host rebuild tracks bounds", test_pane_host_rebuild_tracks_window_bounds},
        {"pane host chrome targets allow compact side panes", test_pane_host_chrome_targets_allow_compact_side_panes},
    };
    return run_test_cases("PaneHost", cases, sizeof(cases) / sizeof(cases[0]));
}
