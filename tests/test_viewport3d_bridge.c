#include "test_framework.h"

#include "Core/viewport3d_bridge.h"

#include <math.h>

static bool viewport3d_bridge_projection_roundtrip(void) {
    FreeViewCamera camera = {true, 35.0f, -20.0f, {4.0f, -3.0f, 2.0f}};
    Grid grid = {2.0f, 3.5f, -1.25f, 8.0f};
    FreeViewCamera normalized_camera = camera;
    Grid normalized_grid = grid;
    CoreViewport3DState state = {0};
    const Vec3 point = {9.0f, 2.0f, -4.0f};
    const Vec2 before_view = Vec3_ProjectToView(point,
                                                (ViewPlane){VIEW_PLANE_XY, 0.0f},
                                                &camera);
    const float before_x = (before_view.x - grid.offsetX) * grid.gridSize * grid.scale;
    const float before_y = (before_view.y - grid.offsetY) * grid.gridSize * grid.scale;
    TEST_ASSERT(LineDrawingViewport3DBridgeStateFromRuntime(&camera, &grid,
                                                            420.0, 260.0,
                                                            0.01, 100.0, &state));
    TEST_ASSERT(LineDrawingViewport3DBridgeCommit(&state, 420.0, 260.0,
                                                  &normalized_camera, &normalized_grid));
    {
        const Vec2 after_view = Vec3_ProjectToView(point,
                                                   (ViewPlane){VIEW_PLANE_XY, 0.0f},
                                                   &normalized_camera);
        const float after_x = (after_view.x - normalized_grid.offsetX) *
                              normalized_grid.gridSize * normalized_grid.scale;
        const float after_y = (after_view.y - normalized_grid.offsetY) *
                              normalized_grid.gridSize * normalized_grid.scale;
        TEST_ASSERT(fabsf(after_x - before_x) < 0.001f);
        TEST_ASSERT(fabsf(after_y - before_y) < 0.001f);
    }
    return true;
}

static bool viewport3d_bridge_pan_zoom_and_nonmutation(void) {
    FreeViewCamera camera = {true, 35.0f, -20.0f, {4.0f, -3.0f, 2.0f}};
    Grid grid = {2.0f, -20.0f, -15.0f, 8.0f};
    FreeViewCamera after_camera = camera;
    Grid after_grid = grid;
    CoreViewport3DCommand command = {0};
    command.kind = CORE_VIEWPORT3D_COMMAND_PAN;
    command.value.pan.screen_dx = 32.0;
    command.value.pan.screen_dy = -16.0;
    TEST_ASSERT(LineDrawingViewport3DBridgeApply(&camera, &grid, 320.0, 240.0,
                                                 1.0, 100.0, &command,
                                                 &after_camera, &after_grid));
    TEST_ASSERT(fabsf(after_camera.target.x - camera.target.x) > 0.001f ||
                fabsf(after_camera.target.y - camera.target.y) > 0.001f ||
                fabsf(after_camera.target.z - camera.target.z) > 0.001f);
    command.kind = CORE_VIEWPORT3D_COMMAND_ZOOM;
    command.value.zoom.factor = 2.0;
    command.value.zoom.anchor_offset_x = 48.0;
    command.value.zoom.anchor_offset_y = -24.0;
    TEST_ASSERT(LineDrawingViewport3DBridgeApply(&camera, &grid, 320.0, 240.0,
                                                 1.0, 100.0, &command,
                                                 &after_camera, &after_grid));
    TEST_ASSERT(fabsf(after_grid.scale - 16.0f) < 0.0001f);
    command.kind = CORE_VIEWPORT3D_COMMAND_PAN;
    command.value.pan.screen_dx = NAN;
    after_camera.target = (Vec3){91.0f, 92.0f, 93.0f};
    after_grid.offsetX = 94.0f;
    TEST_ASSERT(!LineDrawingViewport3DBridgeApply(&camera, &grid, 320.0, 240.0,
                                                  1.0, 100.0, &command,
                                                  &after_camera, &after_grid));
    TEST_ASSERT(fabsf(after_camera.target.x - 91.0f) < 0.0001f);
    TEST_ASSERT(fabsf(after_grid.offsetX - 94.0f) < 0.0001f);
    return true;
}

static bool viewport3d_bridge_orbit_preserves_line_storage(void) {
    const double pi = 3.14159265358979323846;
    FreeViewCamera camera = {true, 5.0f, -20.0f, {7.0f, 8.0f, 9.0f}};
    Grid grid = {20.0f, -3.0f, 4.0f, 2.0f};
    FreeViewCamera after_camera = camera;
    Grid after_grid = grid;
    CoreViewport3DCommand command = {0};
    command.kind = CORE_VIEWPORT3D_COMMAND_ORBIT;
    command.value.orbit.azimuth_delta_rad = -10.0 * pi / 180.0;
    command.value.orbit.elevation_delta_rad = 5.0 * pi / 180.0;
    TEST_ASSERT(LineDrawingViewport3DBridgeApply(&camera, &grid, 400.0, 300.0,
                                                 0.01, 100.0, &command,
                                                 &after_camera, &after_grid));
    TEST_ASSERT(fabsf(after_camera.yawDeg - 355.0f) < 0.0001f);
    TEST_ASSERT(fabsf(after_camera.pitchDeg - (-15.0f)) < 0.0001f);
    TEST_ASSERT(fabsf(after_camera.target.x - camera.target.x) < 0.0001f);
    TEST_ASSERT(fabsf(after_camera.target.y - camera.target.y) < 0.0001f);
    TEST_ASSERT(fabsf(after_camera.target.z - camera.target.z) < 0.0001f);
    TEST_ASSERT(fabsf(after_grid.scale - grid.scale) < 0.0001f);
    TEST_ASSERT(fabsf(after_grid.offsetX - grid.offsetX) < 0.0001f);
    TEST_ASSERT(fabsf(after_grid.offsetY - grid.offsetY) < 0.0001f);
    return true;
}

static bool viewport3d_bridge_resize_preserves_effective_target(void) {
    FreeViewCamera camera = {true, 35.0f, -20.0f, {4.0f, -3.0f, 2.0f}};
    Grid grid = {2.0f, 3.5f, -1.25f, 8.0f};
    FreeViewCamera resized_camera = camera;
    Grid resized_grid = grid;
    CoreViewport3DState before = {0};
    CoreViewport3DState after = {0};
    TEST_ASSERT(LineDrawingViewport3DBridgeStateFromRuntime(
        &camera, &grid, 320.0, 240.0, 0.01, 100.0, &before));
    TEST_ASSERT(LineDrawingViewport3DBridgeApplyResize(
        &camera, &grid, 320.0, 240.0, 500.0, 360.0, 0.01, 100.0,
        &resized_camera, &resized_grid));
    TEST_ASSERT(LineDrawingViewport3DBridgeStateFromRuntime(
        &resized_camera, &resized_grid, 500.0, 360.0, 0.01, 100.0, &after));
    TEST_ASSERT(fabs(after.target.x - before.target.x) < 0.0001);
    TEST_ASSERT(fabs(after.target.y - before.target.y) < 0.0001);
    TEST_ASSERT(fabs(after.target.z - before.target.z) < 0.0001);
    TEST_ASSERT(fabs(after.scale_px_per_world_unit -
                     before.scale_px_per_world_unit) < 0.0001);
    return true;
}

bool viewport3d_bridge_run_tests(void) {
    const TestCase cases[] = {
        {"ProjectionRoundtrip", viewport3d_bridge_projection_roundtrip},
        {"PanZoomAndNonmutation", viewport3d_bridge_pan_zoom_and_nonmutation},
        {"OrbitPreservesLineStorage", viewport3d_bridge_orbit_preserves_line_storage},
        {"ResizePreservesEffectiveTarget",
         viewport3d_bridge_resize_preserves_effective_target}
    };
    return run_test_cases("Viewport3DBridge", cases, sizeof(cases) / sizeof(cases[0]));
}
