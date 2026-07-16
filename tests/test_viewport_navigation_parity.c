#include "test_framework.h"

#include "Core/viewport_navigation_contract.h"

#include <math.h>

static bool evn3_nearly_equal(float actual, float expected) {
    return isfinite(actual) && fabsf(actual - expected) < 0.0001f;
}

static bool test_evn3_pan_matches_canonical_camera_basis_delta(void) {
    const LineDrawingViewportNavState before = {
        .target = { 4.0f, -3.0f, 2.0f },
        .yaw_deg = 35.0f,
        .pitch_deg = -20.0f,
        .zoom_scale = 8.0f,
        .view_offset_x = 0.0f,
        .view_offset_y = 0.0f
    };
    const LineDrawingViewportNavCommand command = {
        .kind = LINE_DRAWING_VIEWPORT_NAV_COMMAND_PAN,
        .screen_dx = 32.0f,
        .screen_dy = -16.0f,
        .grid_size = 2.0f
    };
    const FreeViewCamera camera = {
        .enabled = true,
        .yawDeg = before.yaw_deg,
        .pitchDeg = before.pitch_deg,
        .target = before.target
    };
    const Vec3 right = FreeView_Right(&camera);
    const Vec3 vertical = FreeView_Up(&camera);
    const Vec3 forward = FreeView_Forward(&camera);
    LineDrawingViewportNavState after = {0};
    Vec3 delta = {0};

    TEST_ASSERT(LineDrawingViewportNavApply(&before, &command, &after));
    delta = Vec3_Sub(after.target, before.target);
    TEST_ASSERT(evn3_nearly_equal(Vec3_Dot(delta, right), -2.0f));
    TEST_ASSERT(evn3_nearly_equal(Vec3_Dot(delta, vertical), 1.0f));
    TEST_ASSERT(evn3_nearly_equal(Vec3_Dot(delta, forward), 0.0f));
    TEST_ASSERT(evn3_nearly_equal(after.yaw_deg, before.yaw_deg));
    TEST_ASSERT(evn3_nearly_equal(after.pitch_deg, before.pitch_deg));
    return true;
}

static bool test_evn3_zoom_matches_canonical_anchor_compensation(void) {
    const LineDrawingViewportNavState before = {
        .target = { 4.0f, -3.0f, 2.0f },
        .yaw_deg = 35.0f,
        .pitch_deg = -20.0f,
        .zoom_scale = 8.0f,
        .view_offset_x = 0.0f,
        .view_offset_y = 0.0f
    };
    const LineDrawingViewportNavCommand command = {
        .kind = LINE_DRAWING_VIEWPORT_NAV_COMMAND_ZOOM,
        .zoom_factor = 2.0f,
        .anchor_screen_x = 48.0f,
        .anchor_screen_y = -24.0f,
        .grid_size = 2.0f,
        .min_zoom_scale = 1.0f,
        .max_zoom_scale = 100.0f
    };
    LineDrawingViewportNavState after = {0};

    TEST_ASSERT(LineDrawingViewportNavApply(&before, &command, &after));
    TEST_ASSERT(evn3_nearly_equal(after.zoom_scale, 16.0f));
    TEST_ASSERT(evn3_nearly_equal(after.view_offset_x - before.view_offset_x, 1.5f));
    TEST_ASSERT(evn3_nearly_equal(after.view_offset_y - before.view_offset_y, -0.75f));
    TEST_ASSERT(evn3_nearly_equal(after.target.x, before.target.x));
    TEST_ASSERT(evn3_nearly_equal(after.target.y, before.target.y));
    TEST_ASSERT(evn3_nearly_equal(after.target.z, before.target.z));
    return true;
}

static bool test_evn3_frame_resize_and_invalid_input_contract(void) {
    const LineDrawingViewportNavState before = {
        .target = { 1.0f, 2.0f, 3.0f },
        .yaw_deg = 35.0f,
        .pitch_deg = -20.0f,
        .zoom_scale = 8.0f,
        .view_offset_x = 4.0f,
        .view_offset_y = 5.0f
    };
    LineDrawingViewportNavCommand command = {
        .kind = LINE_DRAWING_VIEWPORT_NAV_COMMAND_FRAME,
        .grid_size = 2.0f,
        .frame_target = { 8.0f, 9.0f, 10.0f },
        .frame_zoom_scale = 16.0f,
        .viewport_center_x = 320.0f,
        .viewport_center_y = 240.0f
    };
    LineDrawingViewportNavState framed = {0};
    LineDrawingViewportNavState resized = {0};
    const LineDrawingViewportNavState sentinel = {
        .target = { 91.0f, 92.0f, 93.0f },
        .yaw_deg = 94.0f,
        .pitch_deg = 95.0f,
        .zoom_scale = 96.0f,
        .view_offset_x = 97.0f,
        .view_offset_y = 98.0f
    };

    TEST_ASSERT(LineDrawingViewportNavApply(&before, &command, &framed));
    TEST_ASSERT(evn3_nearly_equal(framed.target.x, 8.0f));
    TEST_ASSERT(evn3_nearly_equal(framed.target.y, 9.0f));
    TEST_ASSERT(evn3_nearly_equal(framed.target.z, 10.0f));
    TEST_ASSERT(evn3_nearly_equal(framed.zoom_scale, 16.0f));

    command = (LineDrawingViewportNavCommand){
        .kind = LINE_DRAWING_VIEWPORT_NAV_COMMAND_RESIZE
    };
    TEST_ASSERT(LineDrawingViewportNavApply(&framed, &command, &resized));
    TEST_ASSERT(evn3_nearly_equal(resized.target.x, framed.target.x));
    TEST_ASSERT(evn3_nearly_equal(resized.target.y, framed.target.y));
    TEST_ASSERT(evn3_nearly_equal(resized.target.z, framed.target.z));
    TEST_ASSERT(evn3_nearly_equal(resized.zoom_scale, framed.zoom_scale));

    command = (LineDrawingViewportNavCommand){
        .kind = LINE_DRAWING_VIEWPORT_NAV_COMMAND_PAN,
        .screen_dx = NAN,
        .screen_dy = 1.0f,
        .grid_size = 2.0f
    };
    resized = sentinel;
    TEST_ASSERT(!LineDrawingViewportNavApply(&framed, &command, &resized));
    TEST_ASSERT(evn3_nearly_equal(resized.target.x, sentinel.target.x));
    TEST_ASSERT(evn3_nearly_equal(resized.zoom_scale, sentinel.zoom_scale));
    TEST_ASSERT(evn3_nearly_equal(resized.view_offset_x, sentinel.view_offset_x));
    return true;
}

bool viewport_navigation_parity_run_tests(void) {
    const TestCase tests[] = {
        { "EVN3PanMatchesCanonicalCameraBasisDelta",
          test_evn3_pan_matches_canonical_camera_basis_delta },
        { "EVN3ZoomMatchesCanonicalAnchorCompensation",
          test_evn3_zoom_matches_canonical_anchor_compensation },
        { "EVN3FrameResizeAndInvalidInputContract",
          test_evn3_frame_resize_and_invalid_input_contract }
    };
    return run_test_cases("ViewportNavigationParity", tests,
                          sizeof(tests) / sizeof(tests[0]));
}
