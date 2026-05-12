#include "test_framework.h"

#include "Core/viewport_zoom.h"
#include "Editor/space_gizmo_drag.h"
#include "Layout/Grid/grid.h"
#include "Math/math_util.h"

#include <math.h>

static bool nearly_equal(float a, float b) {
    return fabsf(a - b) < 0.0001f;
}

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

static bool test_grid_zoom_clamped_allows_scene_scale_floor(void) {
    Grid grid;
    Grid_init(&grid, 1.0f, 800, 600);
    grid.scale = 32.0f;

    Vec2 beforeAnchorWorld = ScreenToWorld(400, 300, &grid);
    Grid_zoom_clamped(&grid, 0.001f, 400.0f, 300.0f, 0.5f, GRID_DEFAULT_MAX_SCALE);
    Vec2 afterAnchorWorld = ScreenToWorld(400, 300, &grid);

    TEST_ASSERT(nearly_equal(grid.scale, 0.5f));
    TEST_ASSERT(nearly_equal(beforeAnchorWorld.x, afterAnchorWorld.x));
    TEST_ASSERT(nearly_equal(beforeAnchorWorld.y, afterAnchorWorld.y));
    return true;
}

static bool test_viewport_zoom_scene_bounds_min_scale_uses_projected_extents(void) {
    SceneBounds3D bounds = {
        .enabled = true,
        .clampOnEdit = true,
        .min = { -500.0f, -500.0f, -10.0f },
        .max = {  500.0f,  500.0f,  10.0f }
    };
    SpaceViewContext view = {
        .plane = { .axis = VIEW_PLANE_XY, .offset = 0.0f },
        .camera = { .enabled = false }
    };

    float minScale = LineDrawingViewportZoom_MinScaleForSceneBounds(&bounds,
                                                                    &view,
                                                                    800.0f,
                                                                    600.0f,
                                                                    1.0f);
    TEST_ASSERT(nearly_equal(minScale, 0.504f));
    return true;
}

static bool test_viewport_zoom_scene_bounds_min_scale_keeps_default_for_tiny_bounds(void) {
    SceneBounds3D bounds = {
        .enabled = true,
        .clampOnEdit = true,
        .min = { -1.0f, -1.0f, -1.0f },
        .max = {  1.0f,  1.0f,  1.0f }
    };
    SpaceViewContext view = {
        .plane = { .axis = VIEW_PLANE_XY, .offset = 0.0f },
        .camera = { .enabled = false }
    };

    float minScale = LineDrawingViewportZoom_MinScaleForSceneBounds(&bounds,
                                                                    &view,
                                                                    800.0f,
                                                                    600.0f,
                                                                    1.0f);
    TEST_ASSERT(nearly_equal(minScale, GRID_DEFAULT_MIN_SCALE));
    return true;
}

static bool test_vec3_basic_ops(void) {
    Vec3 a = { 1.0f, 2.0f, 3.0f };
    Vec3 b = { -2.0f, 0.5f, 4.0f };

    TEST_ASSERT(nearly_equal(Vec3_Dot(a, b), 11.0f));

    Vec3 c = Vec3_Cross(a, b);
    TEST_ASSERT(nearly_equal(c.x, 6.5f));
    TEST_ASSERT(nearly_equal(c.y, -10.0f));
    TEST_ASSERT(nearly_equal(c.z, 4.5f));
    TEST_ASSERT(nearly_equal(Vec3_Length((Vec3){ 0.0f, 3.0f, 4.0f }), 5.0f));
    return true;
}

static bool test_vec3_normalize_handles_zero(void) {
    Vec3 n = Vec3_Normalize((Vec3){ 0.0f, 3.0f, 4.0f });
    TEST_ASSERT(nearly_equal(n.x, 0.0f));
    TEST_ASSERT(nearly_equal(n.y, 0.6f));
    TEST_ASSERT(nearly_equal(n.z, 0.8f));

    Vec3 zero = Vec3_Normalize((Vec3){ 0.0f, 0.0f, 0.0f });
    TEST_ASSERT(nearly_equal(zero.x, 0.0f));
    TEST_ASSERT(nearly_equal(zero.y, 0.0f));
    TEST_ASSERT(nearly_equal(zero.z, 0.0f));
    return true;
}

static bool test_plane_and_ray_intersection(void) {
    Plane3 plane = Plane3_FromAxisZ(2.0f);
    Ray3 ray = {
        .origin = { 1.0f, 1.0f, -3.0f },
        .direction = Vec3_Normalize((Vec3){ 0.0f, 0.0f, 1.0f })
    };

    float t = -1.0f;
    Vec3 hit = {0};
    TEST_ASSERT(Ray3_IntersectPlane(ray, plane, &t, &hit));
    TEST_ASSERT(nearly_equal(t, 5.0f));
    TEST_ASSERT(nearly_equal(hit.x, 1.0f));
    TEST_ASSERT(nearly_equal(hit.y, 1.0f));
    TEST_ASSERT(nearly_equal(hit.z, 2.0f));
    return true;
}

static bool test_plane_parallel_ray_rejected(void) {
    Plane3 plane = Plane3_FromAxisZ(0.0f);
    Ray3 ray = {
        .origin = { 0.0f, 0.0f, 2.0f },
        .direction = { 1.0f, 0.0f, 0.0f }
    };

    TEST_ASSERT(!Ray3_IntersectPlane(ray, plane, NULL, NULL));
    return true;
}

static bool test_plane_projection_and_frame_roundtrip(void) {
    Plane3 plane = Plane3_FromAxisX(2.0f);
    Vec3 p = { 5.0f, 7.0f, -3.0f };
    float dist = Plane3_SignedDistance(plane, p);
    TEST_ASSERT(nearly_equal(dist, 3.0f));

    Vec3 projected = Plane3_ProjectPoint(plane, p);
    TEST_ASSERT(nearly_equal(projected.x, 2.0f));
    TEST_ASSERT(nearly_equal(projected.y, 7.0f));
    TEST_ASSERT(nearly_equal(projected.z, -3.0f));

    PlaneFrame3 frame = PlaneFrame3_FromPlane(plane, (Vec3){ 2.0f, 0.0f, 0.0f });
    Vec2 uv = PlaneFrame3_ProjectPoint(&frame, projected);
    Vec3 roundtrip = PlaneFrame3_PointFromUV(&frame, uv);

    TEST_ASSERT(nearly_equal(roundtrip.x, projected.x));
    TEST_ASSERT(nearly_equal(roundtrip.y, projected.y));
    TEST_ASSERT(nearly_equal(roundtrip.z, projected.z));
    return true;
}

static bool test_vec2_vec3_adapters(void) {
    Vec2 v2 = { 3.0f, -4.0f };
    Vec3 v3 = Vec3_FromVec2(v2, 9.0f);
    TEST_ASSERT(nearly_equal(v3.x, 3.0f));
    TEST_ASSERT(nearly_equal(v3.y, -4.0f));
    TEST_ASSERT(nearly_equal(v3.z, 9.0f));

    Vec2 back = Vec2_FromVec3(v3);
    TEST_ASSERT(nearly_equal(back.x, v2.x));
    TEST_ASSERT(nearly_equal(back.y, v2.y));
    return true;
}

static bool test_screen_to_plane_world_xy(void) {
    Grid grid;
    Grid_init(&grid, 1.0f, 800, 600);
    Vec2 view = ScreenToSnappedWorld(420, 315, &grid);

    Vec3 world = {0};
    ViewPlane plane = { .axis = VIEW_PLANE_XY, .offset = 5.0f };
    TEST_ASSERT(ScreenToPlaneWorld(420, 315, &grid, plane, NULL, true, &world));
    TEST_ASSERT(nearly_equal(world.x, view.x));
    TEST_ASSERT(nearly_equal(world.y, view.y));
    TEST_ASSERT(nearly_equal(world.z, 5.0f));
    return true;
}

static bool test_screen_to_plane_world_yz(void) {
    Grid grid;
    Grid_init(&grid, 1.0f, 800, 600);
    Vec2 view = ScreenToWorld(420, 315, &grid);

    Vec3 world = {0};
    ViewPlane plane = { .axis = VIEW_PLANE_YZ, .offset = 2.0f };
    TEST_ASSERT(ScreenToPlaneWorld(420, 315, &grid, plane, NULL, false, &world));
    TEST_ASSERT(nearly_equal(world.x, 2.0f));
    TEST_ASSERT(nearly_equal(world.y, view.x));
    TEST_ASSERT(nearly_equal(world.z, view.y));
    return true;
}

static bool test_view_plane_abs_distance_xy(void) {
    ViewPlane plane = { .axis = VIEW_PLANE_XY, .offset = 3.0f };
    TEST_ASSERT(nearly_equal(ViewPlane_AbsDistance(plane, (Vec3){ 1.0f, 2.0f, 3.0f }), 0.0f));
    TEST_ASSERT(nearly_equal(ViewPlane_AbsDistance(plane, (Vec3){ 1.0f, 2.0f, -1.0f }), 4.0f));
    return true;
}

static bool test_view_plane_abs_distance_yz(void) {
    ViewPlane plane = { .axis = VIEW_PLANE_YZ, .offset = -2.0f };
    TEST_ASSERT(nearly_equal(ViewPlane_AbsDistance(plane, (Vec3){ -2.0f, 5.0f, 1.0f }), 0.0f));
    TEST_ASSERT(nearly_equal(ViewPlane_AbsDistance(plane, (Vec3){ 3.0f, 5.0f, 1.0f }), 5.0f));
    return true;
}

static bool test_vec3_project_to_view_free_camera(void) {
    ViewPlane plane = { .axis = VIEW_PLANE_XY, .offset = 0.0f };
    FreeViewCamera cam = {
        .enabled = true,
        .yawDeg = 0.0f,
        .pitchDeg = 0.0f,
        .target = { 0.0f, 0.0f, 0.0f }
    };

    Vec2 v = Vec3_ProjectToView((Vec3){ 0.0f, -2.0f, 3.0f }, plane, &cam);
    TEST_ASSERT(nearly_equal(v.x, 2.0f));
    TEST_ASSERT(nearly_equal(v.y, 3.0f));
    return true;
}

static bool test_screen_to_plane_world_with_free_camera(void) {
    Grid grid;
    Grid_init(&grid, 1.0f, 800, 600);
    ViewPlane plane = { .axis = VIEW_PLANE_YZ, .offset = 0.0f };
    FreeViewCamera cam = {
        .enabled = true,
        .yawDeg = 0.0f,
        .pitchDeg = 0.0f,
        .target = { -5.0f, 0.0f, 0.0f }
    };

    Vec3 world = {0};
    TEST_ASSERT(ScreenToPlaneWorld(400, 300, &grid, plane, &cam, false, &world));
    TEST_ASSERT(nearly_equal(world.x, 0.0f));
    return true;
}

static bool test_handle_polar_roundtrip_xy(void) {
    Vec3 anchor = { 2.0f, 3.0f, 9.0f };
    float inLen = 4.0f;
    float inAngle = 30.0f;

    Vec3 handle = Vec3_HandleWorldPosition(anchor, VIEW_PLANE_XY, inLen, inAngle);
    Vec3 delta = Vec3_Sub(handle, anchor);
    float outLen = 0.0f;
    float outAngle = 0.0f;
    Vec3_HandlePolarFromWorldDelta(delta, VIEW_PLANE_XY, &outLen, &outAngle);

    TEST_ASSERT(nearly_equal(outLen, inLen));
    TEST_ASSERT(nearly_equal(outAngle, inAngle));
    return true;
}

static bool test_handle_polar_roundtrip_yz(void) {
    Vec3 anchor = { -7.0f, 1.0f, 2.0f };
    float inLen = 5.0f;
    float inAngle = -45.0f;

    Vec3 handle = Vec3_HandleWorldPosition(anchor, VIEW_PLANE_YZ, inLen, inAngle);
    Vec3 delta = Vec3_Sub(handle, anchor);
    float outLen = 0.0f;
    float outAngle = 0.0f;
    Vec3_HandlePolarFromWorldDelta(delta, VIEW_PLANE_YZ, &outLen, &outAngle);

    TEST_ASSERT(nearly_equal(outLen, inLen));
    TEST_ASSERT(nearly_equal(outAngle, inAngle));
    TEST_ASSERT(nearly_equal(handle.x, anchor.x));
    return true;
}

static bool test_free_view_orbit_normalize_handles_pole_crossing(void) {
    FreeViewCamera camA = {
        .enabled = true,
        .yawDeg = 15.0f,
        .pitchDeg = 100.0f,
        .target = { 0.0f, 0.0f, 0.0f }
    };
    FreeView_NormalizeOrbitAngles(&camA);
    TEST_ASSERT(nearly_equal(camA.pitchDeg, 100.0f));
    TEST_ASSERT(nearly_equal(camA.yawDeg, 15.0f));

    FreeViewCamera camB = {
        .enabled = true,
        .yawDeg = 20.0f,
        .pitchDeg = -100.0f,
        .target = { 0.0f, 0.0f, 0.0f }
    };
    FreeView_NormalizeOrbitAngles(&camB);
    TEST_ASSERT(nearly_equal(camB.pitchDeg, -100.0f));
    TEST_ASSERT(nearly_equal(camB.yawDeg, 20.0f));

    // Crossing poles should not flip right-vector orientation.
    FreeViewCamera nearPoleA = { .enabled = true, .yawDeg = 40.0f, .pitchDeg = 89.0f };
    FreeViewCamera nearPoleB = { .enabled = true, .yawDeg = 40.0f, .pitchDeg = 91.0f };
    Vec3 rightA = FreeView_Right(&nearPoleA);
    Vec3 rightB = FreeView_Right(&nearPoleB);
    TEST_ASSERT(nearly_equal(rightA.x, rightB.x));
    TEST_ASSERT(nearly_equal(rightA.y, rightB.y));
    TEST_ASSERT(nearly_equal(rightA.z, rightB.z));
    return true;
}

static bool test_gizmo_signed_pixels_axis_projection(void) {
    Vec2 start = { 100.0f, 50.0f };
    Vec2 nowForward = { 128.0f, 50.0f };
    Vec2 nowBackward = { 84.0f, 50.0f };
    Vec2 axis = { 2.0f, 0.0f };

    float forward = GizmoDrag_SignedPixelsAlongAxis(start, nowForward, axis);
    float backward = GizmoDrag_SignedPixelsAlongAxis(start, nowBackward, axis);

    TEST_ASSERT(nearly_equal(forward, 28.0f));
    TEST_ASSERT(nearly_equal(backward, -16.0f));
    return true;
}

static bool test_gizmo_signed_pixels_handles_degenerate_axis(void) {
    Vec2 start = { 10.0f, 10.0f };
    Vec2 now = { 99.0f, 44.0f };
    Vec2 zeroAxis = { 0.0f, 0.0f };

    float signedPixels = GizmoDrag_SignedPixelsAlongAxis(start, now, zeroAxis);
    TEST_ASSERT(nearly_equal(signedPixels, 0.0f));
    return true;
}

static bool test_gizmo_quantized_distance_rounds_signed_steps(void) {
    float step = 0.5f;
    float positive = GizmoDrag_QuantizeDistance(1.26f, step);
    float negative = GizmoDrag_QuantizeDistance(-1.26f, step);

    TEST_ASSERT(nearly_equal(positive, 1.5f));
    TEST_ASSERT(nearly_equal(negative, -1.5f));
    return true;
}

static bool test_gizmo_resolve_distance_smooth_vs_quantized(void) {
    float signedPixels = 14.0f;
    float worldUnitsPerPixel = 0.2f;
    float step = 0.5f;

    float smooth = GizmoDrag_ResolveDistance(signedPixels, worldUnitsPerPixel, step, true);
    float quantized = GizmoDrag_ResolveDistance(signedPixels, worldUnitsPerPixel, step, false);

    TEST_ASSERT(nearly_equal(smooth, 2.8f));
    TEST_ASSERT(nearly_equal(quantized, 3.0f));
    return true;
}

static bool test_gizmo_apply_axis_distance_world_vector(void) {
    Vec3 origin = { 2.0f, -3.0f, 4.0f };

    Vec3 pos = GizmoDrag_ApplyAxisDistance(origin, GIZMO_AXIS_DIR_POS_Z, 1.5f);
    TEST_ASSERT(nearly_equal(pos.x, 2.0f));
    TEST_ASSERT(nearly_equal(pos.y, -3.0f));
    TEST_ASSERT(nearly_equal(pos.z, 5.5f));

    Vec3 neg = GizmoDrag_ApplyAxisDistance(origin, GIZMO_AXIS_DIR_NEG_X, 2.0f);
    TEST_ASSERT(nearly_equal(neg.x, 0.0f));
    TEST_ASSERT(nearly_equal(neg.y, -3.0f));
    TEST_ASSERT(nearly_equal(neg.z, 4.0f));
    return true;
}

bool math_run_tests(void) {
    const TestCase cases[] = {
        { "Vec2SnapAlignsToGrid", test_vec2_snap_aligns_to_grid },
        { "ScreenWorldRoundtrip", test_screen_to_world_roundtrip },
        { "GridZoomClampedAllowsSceneScaleFloor", test_grid_zoom_clamped_allows_scene_scale_floor },
        { "ViewportZoomSceneBoundsMinScaleUsesProjectedExtents",
          test_viewport_zoom_scene_bounds_min_scale_uses_projected_extents },
        { "ViewportZoomSceneBoundsMinScaleKeepsDefaultForTinyBounds",
          test_viewport_zoom_scene_bounds_min_scale_keeps_default_for_tiny_bounds },
        { "Vec3BasicOps", test_vec3_basic_ops },
        { "Vec3NormalizeHandlesZero", test_vec3_normalize_handles_zero },
        { "PlaneAndRayIntersection", test_plane_and_ray_intersection },
        { "PlaneParallelRayRejected", test_plane_parallel_ray_rejected },
        { "PlaneProjectionAndFrameRoundtrip", test_plane_projection_and_frame_roundtrip },
        { "Vec2Vec3Adapters", test_vec2_vec3_adapters },
        { "ScreenToPlaneWorldXY", test_screen_to_plane_world_xy },
        { "ScreenToPlaneWorldYZ", test_screen_to_plane_world_yz },
        { "ViewPlaneAbsDistanceXY", test_view_plane_abs_distance_xy },
        { "ViewPlaneAbsDistanceYZ", test_view_plane_abs_distance_yz },
        { "Vec3ProjectToViewFreeCamera", test_vec3_project_to_view_free_camera },
        { "ScreenToPlaneWorldWithFreeCamera", test_screen_to_plane_world_with_free_camera },
        { "HandlePolarRoundtripXY", test_handle_polar_roundtrip_xy },
        { "HandlePolarRoundtripYZ", test_handle_polar_roundtrip_yz },
        { "FreeViewOrbitNormalizeHandlesPoleCrossing", test_free_view_orbit_normalize_handles_pole_crossing },
        { "GizmoSignedPixelsAxisProjection", test_gizmo_signed_pixels_axis_projection },
        { "GizmoSignedPixelsHandlesDegenerateAxis", test_gizmo_signed_pixels_handles_degenerate_axis },
        { "GizmoQuantizedDistanceRoundsSignedSteps", test_gizmo_quantized_distance_rounds_signed_steps },
        { "GizmoResolveDistanceSmoothVsQuantized", test_gizmo_resolve_distance_smooth_vs_quantized },
        { "GizmoApplyAxisDistanceWorldVector", test_gizmo_apply_axis_distance_world_vector }
    };

    return run_test_cases("Math", cases, sizeof(cases) / sizeof(cases[0]));
}
