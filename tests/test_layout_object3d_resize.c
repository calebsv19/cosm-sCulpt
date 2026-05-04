#include "test_layout_internal.h"

static bool test_plane_resize_corner_drag_updates_size_and_center(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    PlanePrimitiveCreateParams params;
    Layout_PlanePrimitiveCreateParams_SetDefaults(&params);
    params.width = 4.0f;
    params.height = 4.0f;
    params.lockToBounds = false;

    uint32_t objectId = 0u;
    bool adjusted = false;
    TEST_ASSERT(Layout_CreatePlanePrimitive(layout, &params, &objectId, &adjusted));
    Object3D* object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);

    const Vec3 startCenter = object->transform.position;
    const Vec3 axisU = Vec3_Normalize(object->plane.frame.axisU);
    const Vec3 axisV = Vec3_Normalize(object->plane.frame.axisV);
    const float fixedU = -object->plane.width * 0.5f;
    const float fixedV = -object->plane.height * 0.5f;
    const float movedU = 5.0f;
    const float movedV = 3.0f;
    Vec3 dragPoint = Vec3_Add(startCenter,
                              Vec3_Add(Vec3_Scale(axisU, movedU),
                                       Vec3_Scale(axisV, movedV)));

    TEST_ASSERT(Layout_ResizePlanePrimitiveFromHandle(layout,
                                                      objectId,
                                                      PLANE_RESIZE_HANDLE_CORNER_POS_U_POS_V,
                                                      dragPoint,
                                                      &adjusted));
    TEST_ASSERT(!adjusted);

    object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(ld_test_nearly_equal(object->plane.width, movedU - fixedU));
    TEST_ASSERT(ld_test_nearly_equal(object->plane.height, movedV - fixedV));
    {
        Vec3 expectedCenter = Vec3_Add(startCenter,
                                       Vec3_Add(Vec3_Scale(axisU, (fixedU + movedU) * 0.5f),
                                                Vec3_Scale(axisV, (fixedV + movedV) * 0.5f)));
        TEST_ASSERT(ld_test_vec3_nearly_equal(object->transform.position, expectedCenter));
    }

    ld_test_shutdown_runtime();
    return true;
}

static bool test_plane_resize_edge_drag_keeps_opposite_edge_fixed(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    PlanePrimitiveCreateParams params;
    Layout_PlanePrimitiveCreateParams_SetDefaults(&params);
    params.width = 4.0f;
    params.height = 4.0f;
    params.lockToBounds = false;

    uint32_t objectId = 0u;
    TEST_ASSERT(Layout_CreatePlanePrimitive(layout, &params, &objectId, NULL));
    Object3D* object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);

    const Vec3 startCenter = object->transform.position;
    const Vec3 axisU = Vec3_Normalize(object->plane.frame.axisU);
    const float fixedU = -object->plane.width * 0.5f;
    const float movedU = 1.0f;
    Vec3 dragPoint = Vec3_Add(startCenter, Vec3_Scale(axisU, movedU));

    TEST_ASSERT(Layout_ResizePlanePrimitiveFromHandle(layout,
                                                      objectId,
                                                      PLANE_RESIZE_HANDLE_EDGE_POS_U,
                                                      dragPoint,
                                                      NULL));
    object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(ld_test_nearly_equal(object->plane.width, movedU - fixedU));
    TEST_ASSERT(ld_test_nearly_equal(object->plane.height, 4.0f));
    {
        Vec3 expectedCenter = Vec3_Add(startCenter, Vec3_Scale(axisU, (fixedU + movedU) * 0.5f));
        TEST_ASSERT(ld_test_vec3_nearly_equal(object->transform.position, expectedCenter));
    }

    ld_test_shutdown_runtime();
    return true;
}

static bool test_plane_resize_edge_drag_clamps_to_min_size(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    PlanePrimitiveCreateParams params;
    Layout_PlanePrimitiveCreateParams_SetDefaults(&params);
    params.width = 4.0f;
    params.height = 4.0f;
    params.lockToBounds = false;

    uint32_t objectId = 0u;
    TEST_ASSERT(Layout_CreatePlanePrimitive(layout, &params, &objectId, NULL));
    Object3D* object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);

    const Vec3 startCenter = object->transform.position;
    const Vec3 axisU = Vec3_Normalize(object->plane.frame.axisU);
    const float fixedU = -object->plane.width * 0.5f;
    const float minSize = Layout_PlanePrimitiveMinSize();
    const float draggedU = fixedU - (minSize * 0.25f);
    Vec3 dragPoint = Vec3_Add(startCenter, Vec3_Scale(axisU, draggedU));

    TEST_ASSERT(Layout_ResizePlanePrimitiveFromHandle(layout,
                                                      objectId,
                                                      PLANE_RESIZE_HANDLE_EDGE_POS_U,
                                                      dragPoint,
                                                      NULL));
    object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(object->plane.width > minSize);
    TEST_ASSERT(object->plane.width < (minSize + 0.0002f));
    TEST_ASSERT(ld_test_nearly_equal(object->plane.height, 4.0f));
    {
        const float movedU = fixedU - object->plane.width;
        Vec3 expectedCenter = Vec3_Add(startCenter, Vec3_Scale(axisU, (fixedU + movedU) * 0.5f));
        TEST_ASSERT(ld_test_vec3_nearly_equal(object->transform.position, expectedCenter));
    }

    ld_test_shutdown_runtime();
    return true;
}

static bool test_plane_resize_corner_drag_crosses_anchor_and_flips_axes(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    PlanePrimitiveCreateParams params;
    Layout_PlanePrimitiveCreateParams_SetDefaults(&params);
    params.width = 4.0f;
    params.height = 4.0f;
    params.lockToBounds = false;

    uint32_t objectId = 0u;
    TEST_ASSERT(Layout_CreatePlanePrimitive(layout, &params, &objectId, NULL));
    Object3D* object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);

    const Vec3 startCenter = object->transform.position;
    const Vec3 axisU = Vec3_Normalize(object->plane.frame.axisU);
    const Vec3 axisV = Vec3_Normalize(object->plane.frame.axisV);
    const float fixedU = -2.0f;
    const float fixedV = -2.0f;
    const float movedU = -5.0f;
    const float movedV = -3.0f;
    Vec3 dragPoint = Vec3_Add(startCenter,
                              Vec3_Add(Vec3_Scale(axisU, movedU),
                                       Vec3_Scale(axisV, movedV)));

    TEST_ASSERT(Layout_ResizePlanePrimitiveFromHandle(layout,
                                                      objectId,
                                                      PLANE_RESIZE_HANDLE_CORNER_POS_U_POS_V,
                                                      dragPoint,
                                                      NULL));
    object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(ld_test_nearly_equal(object->plane.width, fabsf(movedU - fixedU)));
    TEST_ASSERT(ld_test_nearly_equal(object->plane.height, fabsf(movedV - fixedV)));
    {
        Vec3 expectedCenter = Vec3_Add(startCenter,
                                       Vec3_Add(Vec3_Scale(axisU, (fixedU + movedU) * 0.5f),
                                                Vec3_Scale(axisV, (fixedV + movedV) * 0.5f)));
        TEST_ASSERT(ld_test_vec3_nearly_equal(object->transform.position, expectedCenter));
    }

    ld_test_shutdown_runtime();
    return true;
}

static bool test_plane_resize_handle_resolution_flips_corner_axes(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    PlanePrimitiveCreateParams params;
    Layout_PlanePrimitiveCreateParams_SetDefaults(&params);
    params.width = 4.0f;
    params.height = 4.0f;
    params.lockToBounds = false;

    uint32_t objectId = 0u;
    TEST_ASSERT(Layout_CreatePlanePrimitive(layout, &params, &objectId, NULL));
    Object3D* object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);

    const Vec3 center = object->transform.position;
    const Vec3 axisU = Vec3_Normalize(object->plane.frame.axisU);
    const Vec3 axisV = Vec3_Normalize(object->plane.frame.axisV);

    {
        Vec3 dragPoint = Vec3_Add(center,
                                  Vec3_Add(Vec3_Scale(axisU, -5.0f),
                                           Vec3_Scale(axisV, 1.0f)));
        PlaneResizeHandleKind resolved =
            Layout_ResolvePlaneResizeHandleForDrag(object,
                                                   PLANE_RESIZE_HANDLE_CORNER_POS_U_POS_V,
                                                   dragPoint);
        TEST_ASSERT(resolved == PLANE_RESIZE_HANDLE_CORNER_NEG_U_POS_V);
    }
    {
        Vec3 dragPoint = Vec3_Add(center,
                                  Vec3_Add(Vec3_Scale(axisU, -5.0f),
                                           Vec3_Scale(axisV, -3.0f)));
        PlaneResizeHandleKind resolved =
            Layout_ResolvePlaneResizeHandleForDrag(object,
                                                   PLANE_RESIZE_HANDLE_CORNER_POS_U_POS_V,
                                                   dragPoint);
        TEST_ASSERT(resolved == PLANE_RESIZE_HANDLE_CORNER_NEG_U_NEG_V);
    }

    ld_test_shutdown_runtime();
    return true;
}

static bool test_plane_resize_handle_resolution_flips_edge_axis(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    PlanePrimitiveCreateParams params;
    Layout_PlanePrimitiveCreateParams_SetDefaults(&params);
    params.width = 4.0f;
    params.height = 4.0f;
    params.lockToBounds = false;

    uint32_t objectId = 0u;
    TEST_ASSERT(Layout_CreatePlanePrimitive(layout, &params, &objectId, NULL));
    Object3D* object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);

    const Vec3 center = object->transform.position;
    const Vec3 axisU = Vec3_Normalize(object->plane.frame.axisU);
    Vec3 dragPoint = Vec3_Add(center, Vec3_Scale(axisU, -5.0f));
    PlaneResizeHandleKind resolved =
        Layout_ResolvePlaneResizeHandleForDrag(object,
                                               PLANE_RESIZE_HANDLE_EDGE_POS_U,
                                               dragPoint);
    TEST_ASSERT(resolved == PLANE_RESIZE_HANDLE_EDGE_NEG_U);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_rect_prism_resize_corner_drag_updates_size_and_center(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    RectPrismPrimitiveCreateParams params;
    Layout_RectPrismPrimitiveCreateParams_SetDefaults(&params);
    params.width = 4.0f;
    params.height = 4.0f;
    params.depth = 6.0f;
    params.lockToBounds = false;

    uint32_t objectId = 0u;
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(layout, &params, &objectId, NULL));
    Object3D* object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);

    const Vec3 startCenter = object->transform.position;
    const Vec3 axisU = Vec3_Normalize(object->rectPrism.frame.axisU);
    const Vec3 axisV = Vec3_Normalize(object->rectPrism.frame.axisV);
    const float fixedU = -2.0f;
    const float fixedV = -2.0f;
    const float movedU = 5.0f;
    const float movedV = 3.0f;
    Vec3 dragPoint = Vec3_Add(startCenter,
                              Vec3_Add(Vec3_Scale(axisU, movedU),
                                       Vec3_Scale(axisV, movedV)));

    TEST_ASSERT(Layout_ResizeRectPrismPrimitiveFromHandle(layout,
                                                          objectId,
                                                          PLANE_RESIZE_HANDLE_CORNER_POS_U_POS_V,
                                                          dragPoint,
                                                          NULL));
    object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(ld_test_nearly_equal(object->rectPrism.width, movedU - fixedU));
    TEST_ASSERT(ld_test_nearly_equal(object->rectPrism.height, movedV - fixedV));
    TEST_ASSERT(ld_test_nearly_equal(object->rectPrism.depth, 6.0f));
    {
        Vec3 expectedCenter = Vec3_Add(startCenter,
                                       Vec3_Add(Vec3_Scale(axisU, (fixedU + movedU) * 0.5f),
                                                Vec3_Scale(axisV, (fixedV + movedV) * 0.5f)));
        TEST_ASSERT(ld_test_vec3_nearly_equal(object->transform.position, expectedCenter));
    }

    ld_test_shutdown_runtime();
    return true;
}

static bool test_rect_prism_resize_handle_resolution_flips_corner_axes(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    RectPrismPrimitiveCreateParams params;
    Layout_RectPrismPrimitiveCreateParams_SetDefaults(&params);
    params.width = 4.0f;
    params.height = 4.0f;
    params.depth = 4.0f;
    params.lockToBounds = false;

    uint32_t objectId = 0u;
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(layout, &params, &objectId, NULL));
    Object3D* object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);

    const Vec3 center = object->transform.position;
    const Vec3 axisU = Vec3_Normalize(object->rectPrism.frame.axisU);
    const Vec3 axisV = Vec3_Normalize(object->rectPrism.frame.axisV);
    Vec3 dragPoint = Vec3_Add(center,
                              Vec3_Add(Vec3_Scale(axisU, -5.0f),
                                       Vec3_Scale(axisV, 1.0f)));
    PlaneResizeHandleKind resolved =
        Layout_ResolveRectPrismResizeHandleForDrag(object,
                                                   PLANE_RESIZE_HANDLE_CORNER_POS_U_POS_V,
                                                   dragPoint);
    TEST_ASSERT(resolved == PLANE_RESIZE_HANDLE_CORNER_NEG_U_POS_V);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_rect_prism_depth_resize_from_face_handle_updates_depth_and_center(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    RectPrismPrimitiveCreateParams params;
    Layout_RectPrismPrimitiveCreateParams_SetDefaults(&params);
    params.width = 4.0f;
    params.height = 4.0f;
    params.depth = 4.0f;
    params.lockToBounds = false;

    uint32_t objectId = 0u;
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(layout, &params, &objectId, NULL));
    Object3D* object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);

    const Vec3 startCenter = object->transform.position;
    const Vec3 axisN = Vec3_Normalize(object->rectPrism.frame.normal);
    const float fixedN = -2.0f;
    const float movedN = 5.0f;
    Vec3 dragPoint = Vec3_Add(startCenter, Vec3_Scale(axisN, movedN));

    TEST_ASSERT(Layout_ResizeRectPrismDepthFromFaceHandle(layout,
                                                          objectId,
                                                          PLANE_RESIZE_HANDLE_CORNER_POS_U_POS_V,
                                                          true,
                                                          dragPoint,
                                                          NULL));
    object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(ld_test_nearly_equal(object->rectPrism.width, 4.0f));
    TEST_ASSERT(ld_test_nearly_equal(object->rectPrism.height, 4.0f));
    TEST_ASSERT(ld_test_nearly_equal(object->rectPrism.depth, movedN - fixedN));
    {
        Vec3 expectedCenter = Vec3_Add(startCenter, Vec3_Scale(axisN, (fixedN + movedN) * 0.5f));
        TEST_ASSERT(ld_test_vec3_nearly_equal(object->transform.position, expectedCenter));
    }

    ld_test_shutdown_runtime();
    return true;
}

static bool test_rect_prism_set_dimensions_preserves_center_and_updates_size(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    RectPrismPrimitiveCreateParams params;
    Layout_RectPrismPrimitiveCreateParams_SetDefaults(&params);
    params.width = 4.0f;
    params.height = 3.0f;
    params.depth = 2.0f;
    params.lockToBounds = false;

    uint32_t objectId = 0u;
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(layout, &params, &objectId, NULL));
    Object3D* object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);
    Vec3 centerBefore = object->transform.position;

    TEST_ASSERT(Layout_SetRectPrismDimensions(layout, objectId, 9.0f, 8.0f, 0.0f, NULL));
    object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(ld_test_nearly_equal(object->rectPrism.width, 9.0f));
    TEST_ASSERT(ld_test_nearly_equal(object->rectPrism.height, 8.0f));
    TEST_ASSERT(ld_test_nearly_equal(object->rectPrism.depth, 0.0f));
    TEST_ASSERT(ld_test_vec3_nearly_equal(object->transform.position, centerBefore));
    TEST_ASSERT(Layout_ObjectStore_ValidateObject(object));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_rect_prism_set_dimensions_clamps_to_bounds_when_locked(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    layout->scene3d.bounds.enabled = true;
    layout->scene3d.bounds.clampOnEdit = true;
    layout->scene3d.bounds.min = (Vec3){ -2.0f, -2.0f, -2.0f };
    layout->scene3d.bounds.max = (Vec3){ 2.0f, 2.0f, 2.0f };

    RectPrismPrimitiveCreateParams params;
    Layout_RectPrismPrimitiveCreateParams_SetDefaults(&params);
    params.width = 1.0f;
    params.height = 1.0f;
    params.depth = 1.0f;
    params.lockToBounds = true;

    uint32_t objectId = 0u;
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(layout, &params, &objectId, NULL));
    Object3D* object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);

    bool boundsAdjusted = false;
    TEST_ASSERT(Layout_SetRectPrismDimensions(layout,
                                              objectId,
                                              999.0f,
                                              999.0f,
                                              999.0f,
                                              &boundsAdjusted));
    TEST_ASSERT(boundsAdjusted);

    object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(ld_test_nearly_equal(object->rectPrism.width, 4.0f));
    TEST_ASSERT(ld_test_nearly_equal(object->rectPrism.height, 4.0f));
    TEST_ASSERT(ld_test_nearly_equal(object->rectPrism.depth, 4.0f));
    TEST_ASSERT(Layout_ObjectStore_ValidateObject(object));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_rect_prism_set_dimensions_rejects_invalid_values(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    RectPrismPrimitiveCreateParams params;
    Layout_RectPrismPrimitiveCreateParams_SetDefaults(&params);
    params.width = 4.0f;
    params.height = 4.0f;
    params.depth = 2.0f;
    params.lockToBounds = false;

    uint32_t objectId = 0u;
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(layout, &params, &objectId, NULL));

    TEST_ASSERT(!Layout_SetRectPrismDimensions(layout, objectId, 0.0f, 4.0f, 1.0f, NULL));
    TEST_ASSERT(!Layout_SetRectPrismDimensions(layout, objectId, 4.0f, 0.0f, 1.0f, NULL));
    TEST_ASSERT(!Layout_SetRectPrismDimensions(layout, objectId, 4.0f, 4.0f, -1.0f, NULL));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_set_object3d_position_moves_plane_and_preserves_dimensions(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    PlanePrimitiveCreateParams params;
    Layout_PlanePrimitiveCreateParams_SetDefaults(&params);
    params.width = 6.0f;
    params.height = 2.0f;
    params.lockToConstructionPlane = false;
    params.lockToBounds = false;

    uint32_t objectId = 0u;
    TEST_ASSERT(Layout_CreatePlanePrimitive(layout, &params, &objectId, NULL));
    Object3D* object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(object->kind == OBJECT3D_KIND_PLANE);

    TEST_ASSERT(Layout_SetObject3DPosition(layout,
                                           objectId,
                                           (Vec3){ 7.0f, -3.0f, 4.0f },
                                           NULL));
    TEST_ASSERT(ld_test_vec3_nearly_equal(object->transform.position, (Vec3){ 7.0f, -3.0f, 4.0f }));
    TEST_ASSERT(ld_test_vec3_nearly_equal(object->plane.frame.origin, object->transform.position));
    TEST_ASSERT(ld_test_nearly_equal(object->plane.width, 6.0f));
    TEST_ASSERT(ld_test_nearly_equal(object->plane.height, 2.0f));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_set_object3d_position_clamps_locked_prism_to_bounds(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    layout->scene3d.bounds.enabled = true;
    layout->scene3d.bounds.min = (Vec3){ -5.0f, -5.0f, -5.0f };
    layout->scene3d.bounds.max = (Vec3){  5.0f,  5.0f,  5.0f };

    RectPrismPrimitiveCreateParams params;
    Layout_RectPrismPrimitiveCreateParams_SetDefaults(&params);
    params.width = 4.0f;
    params.height = 2.0f;
    params.depth = 2.0f;
    params.lockToConstructionPlane = false;
    params.lockToBounds = true;

    uint32_t objectId = 0u;
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(layout, &params, &objectId, NULL));
    Object3D* object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(object->kind == OBJECT3D_KIND_RECT_PRISM);

    bool adjusted = false;
    TEST_ASSERT(Layout_SetObject3DPosition(layout,
                                           objectId,
                                           (Vec3){ 10.0f, 0.0f, 0.0f },
                                           &adjusted));
    TEST_ASSERT(adjusted);
    TEST_ASSERT(object->transform.position.x > 0.0f);
    TEST_ASSERT(object->transform.position.x <= layout->scene3d.bounds.max.x);
    TEST_ASSERT(object->transform.position.x >= layout->scene3d.bounds.min.x);
    TEST_ASSERT(ld_test_vec3_nearly_equal(object->rectPrism.frame.origin, object->transform.position));
    {
        Vec3 corners[8] = {0};
        TEST_ASSERT(Layout_Object3D_ComputeRectPrismCorners(object, corners));
        for (int i = 0; i < 8; ++i) {
            TEST_ASSERT(corners[i].x <= layout->scene3d.bounds.max.x + 1e-4f);
            TEST_ASSERT(corners[i].x >= layout->scene3d.bounds.min.x - 1e-4f);
            TEST_ASSERT(corners[i].y <= layout->scene3d.bounds.max.y + 1e-4f);
            TEST_ASSERT(corners[i].y >= layout->scene3d.bounds.min.y - 1e-4f);
            TEST_ASSERT(corners[i].z <= layout->scene3d.bounds.max.z + 1e-4f);
            TEST_ASSERT(corners[i].z >= layout->scene3d.bounds.min.z - 1e-4f);
        }
    }

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_rotate_object3d_rotates_plane_frame_and_preserves_dimensions(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    PlanePrimitiveCreateParams params;
    Layout_PlanePrimitiveCreateParams_SetDefaults(&params);
    params.width = 6.0f;
    params.height = 2.0f;
    params.lockToConstructionPlane = false;
    params.lockToBounds = false;

    uint32_t objectId = 0u;
    TEST_ASSERT(Layout_CreatePlanePrimitive(layout, &params, &objectId, NULL));
    Object3D* object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(object->kind == OBJECT3D_KIND_PLANE);

    Object3D baseline = *object;
    Vec3 startU = Vec3_Normalize(baseline.plane.frame.axisU);
    Vec3 startV = Vec3_Normalize(baseline.plane.frame.axisV);
    Vec3 startN = Vec3_Normalize(baseline.plane.frame.normal);

    TEST_ASSERT(Layout_RotateObject3D(layout,
                                      objectId,
                                      (Vec3){ 0.0f, 0.0f, 1.0f },
                                      90.0f,
                                      &baseline,
                                      NULL));

    Vec3 nextU = Vec3_Normalize(object->plane.frame.axisU);
    Vec3 nextV = Vec3_Normalize(object->plane.frame.axisV);
    Vec3 nextN = Vec3_Normalize(object->plane.frame.normal);

    TEST_ASSERT(ld_test_nearly_equal(object->plane.width, 6.0f));
    TEST_ASSERT(ld_test_nearly_equal(object->plane.height, 2.0f));
    TEST_ASSERT(ld_test_vec3_nearly_equal(object->plane.frame.origin, object->transform.position));
    TEST_ASSERT(fabsf(Vec3_Dot(startU, nextU)) < 0.01f);
    TEST_ASSERT(fabsf(Vec3_Dot(startV, nextV)) < 0.01f);
    TEST_ASSERT(fabsf(Vec3_Dot(startN, nextN)) > 0.99f);
    TEST_ASSERT(fabsf(Vec3_Dot(nextU, nextV)) < 0.01f);
    TEST_ASSERT(fabsf(Vec3_Dot(nextU, nextN)) < 0.01f);
    TEST_ASSERT(fabsf(Vec3_Dot(nextV, nextN)) < 0.01f);
    TEST_ASSERT(ld_test_nearly_equal(object->transform.rotationDeg.z, 90.0f));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_rotate_object3d_baseline_composition_is_deterministic(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    RectPrismPrimitiveCreateParams params;
    Layout_RectPrismPrimitiveCreateParams_SetDefaults(&params);
    params.width = 4.0f;
    params.height = 3.0f;
    params.depth = 2.0f;
    params.lockToConstructionPlane = false;
    params.lockToBounds = false;

    uint32_t idA = 0u;
    uint32_t idB = 0u;
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(layout, &params, &idA, NULL));
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(layout, &params, &idB, NULL));

    Object3D* objectA = Layout_ObjectStore_Find(&layout->objectStore, idA);
    Object3D* objectB = Layout_ObjectStore_Find(&layout->objectStore, idB);
    TEST_ASSERT(objectA != NULL);
    TEST_ASSERT(objectB != NULL);

    Object3D baseA = *objectA;
    Object3D baseB = *objectB;

    TEST_ASSERT(Layout_RotateObject3D(layout,
                                      idA,
                                      (Vec3){ 0.0f, 0.0f, 1.0f },
                                      15.0f,
                                      &baseA,
                                      NULL));
    TEST_ASSERT(Layout_RotateObject3D(layout,
                                      idA,
                                      (Vec3){ 0.0f, 0.0f, 1.0f },
                                      75.0f,
                                      &baseA,
                                      NULL));
    TEST_ASSERT(Layout_RotateObject3D(layout,
                                      idB,
                                      (Vec3){ 0.0f, 0.0f, 1.0f },
                                      75.0f,
                                      &baseB,
                                      NULL));

    TEST_ASSERT(ld_test_vec3_nearly_equal(objectA->rectPrism.frame.axisU, objectB->rectPrism.frame.axisU));
    TEST_ASSERT(ld_test_vec3_nearly_equal(objectA->rectPrism.frame.axisV, objectB->rectPrism.frame.axisV));
    TEST_ASSERT(ld_test_vec3_nearly_equal(objectA->rectPrism.frame.normal, objectB->rectPrism.frame.normal));
    TEST_ASSERT(ld_test_vec3_nearly_equal(objectA->transform.rotationDeg, objectB->transform.rotationDeg));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_rotate_object3d_clamps_locked_bounds(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    layout->scene3d.bounds.enabled = true;
    layout->scene3d.bounds.clampOnEdit = true;
    layout->scene3d.bounds.min = (Vec3){ -5.0f, -5.0f, -5.0f };
    layout->scene3d.bounds.max = (Vec3){  5.0f,  5.0f,  5.0f };

    RectPrismPrimitiveCreateParams params;
    Layout_RectPrismPrimitiveCreateParams_SetDefaults(&params);
    params.width = 4.0f;
    params.height = 2.0f;
    params.depth = 2.0f;
    params.lockToConstructionPlane = false;
    params.lockToBounds = true;

    uint32_t objectId = 0u;
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(layout, &params, &objectId, NULL));
    TEST_ASSERT(Layout_SetObject3DPosition(layout,
                                           objectId,
                                           (Vec3){ 3.0f, 0.0f, 0.0f },
                                           NULL));

    Object3D* object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);
    const float centerBefore = object->transform.position.x;
    Object3D baseline = *object;

    bool adjusted = false;
    TEST_ASSERT(Layout_RotateObject3D(layout,
                                      objectId,
                                      (Vec3){ 0.0f, 0.0f, 1.0f },
                                      45.0f,
                                      &baseline,
                                      &adjusted));
    TEST_ASSERT(adjusted);
    TEST_ASSERT(object->transform.position.x < centerBefore);

    Vec3 corners[8] = {0};
    TEST_ASSERT(Layout_Object3D_ComputeRectPrismCorners(object, corners));
    for (int i = 0; i < 8; ++i) {
        TEST_ASSERT(corners[i].x <= layout->scene3d.bounds.max.x + 1e-4f);
        TEST_ASSERT(corners[i].x >= layout->scene3d.bounds.min.x - 1e-4f);
        TEST_ASSERT(corners[i].y <= layout->scene3d.bounds.max.y + 1e-4f);
        TEST_ASSERT(corners[i].y >= layout->scene3d.bounds.min.y - 1e-4f);
        TEST_ASSERT(corners[i].z <= layout->scene3d.bounds.max.z + 1e-4f);
        TEST_ASSERT(corners[i].z >= layout->scene3d.bounds.min.z - 1e-4f);
    }

    ld_test_shutdown_runtime();
    return true;
}

static bool test_ui_panel_display_unit_conversion_roundtrip(void) {
    ld_test_init_runtime();

    TEST_ASSERT(UIPanel_SetDisplayUnit(CORE_UNIT_FOOT));
    {
        double displayValue = 0.0;
        double worldValue = 0.0;
        TEST_ASSERT(UIPanel_ConvertWorldToDisplay(1.0, &displayValue));
        TEST_ASSERT(UIPanel_ConvertDisplayToWorld(displayValue, &worldValue));
        TEST_ASSERT(fabs(worldValue - 1.0) < 1e-6);
    }

    TEST_ASSERT(UIPanel_SetDisplayUnit(CORE_UNIT_MILLIMETER));
    {
        double displayValue = 0.0;
        double worldValue = 0.0;
        TEST_ASSERT(UIPanel_ConvertWorldToDisplay(0.125, &displayValue));
        TEST_ASSERT(UIPanel_ConvertDisplayToWorld(displayValue, &worldValue));
        TEST_ASSERT(fabs(worldValue - 0.125) < 1e-6);
    }

    ld_test_shutdown_runtime();
    return true;
}

static bool test_ui_panel_display_unit_rounding_determinism(void) {
    ld_test_init_runtime();
    TEST_ASSERT(UIPanel_SetDisplayUnit(CORE_UNIT_INCH));

    {
        double displayValueA = 0.0;
        double worldFromTyped = 0.0;
        double displayValueB = 0.0;
        char typedA[64] = {0};
        char typedB[64] = {0};

        TEST_ASSERT(UIPanel_ConvertWorldToDisplay(2.75, &displayValueA));
        snprintf(typedA, sizeof(typedA), "%.3f", displayValueA);
        TEST_ASSERT(core_units_unit_to_world(strtod(typedA, NULL),
                                             CORE_UNIT_INCH,
                                             1.0,
                                             &worldFromTyped).code == CORE_OK);
        TEST_ASSERT(UIPanel_ConvertWorldToDisplay(worldFromTyped, &displayValueB));
        snprintf(typedB, sizeof(typedB), "%.3f", displayValueB);
        TEST_ASSERT(strcmp(typedA, typedB) == 0);
    }

    ld_test_shutdown_runtime();
    return true;
}

bool test_layout_object3d_resize_run_tests(void) {
    const TestCase cases[] = {
        { "PlaneResizeCornerDragUpdatesSizeAndCenter", test_plane_resize_corner_drag_updates_size_and_center },
        { "PlaneResizeEdgeDragKeepsOppositeEdgeFixed", test_plane_resize_edge_drag_keeps_opposite_edge_fixed },
        { "PlaneResizeEdgeDragClampsToMinSize", test_plane_resize_edge_drag_clamps_to_min_size },
        { "PlaneResizeCornerDragCrossesAnchorAndFlipsAxes", test_plane_resize_corner_drag_crosses_anchor_and_flips_axes },
        { "PlaneResizeHandleResolutionFlipsCornerAxes", test_plane_resize_handle_resolution_flips_corner_axes },
        { "PlaneResizeHandleResolutionFlipsEdgeAxis", test_plane_resize_handle_resolution_flips_edge_axis },
        { "RectPrismResizeCornerDragUpdatesSizeAndCenter", test_rect_prism_resize_corner_drag_updates_size_and_center },
        { "RectPrismResizeHandleResolutionFlipsCornerAxes", test_rect_prism_resize_handle_resolution_flips_corner_axes },
        { "RectPrismDepthResizeFromFaceHandleUpdatesDepthAndCenter", test_rect_prism_depth_resize_from_face_handle_updates_depth_and_center },
        { "RectPrismSetDimensionsPreservesCenterAndUpdatesSize", test_rect_prism_set_dimensions_preserves_center_and_updates_size },
        { "RectPrismSetDimensionsClampsToBoundsWhenLocked", test_rect_prism_set_dimensions_clamps_to_bounds_when_locked },
        { "RectPrismSetDimensionsRejectsInvalidValues", test_rect_prism_set_dimensions_rejects_invalid_values },
        { "LayoutSetObject3DPositionMovesPlaneAndPreservesDimensions", test_layout_set_object3d_position_moves_plane_and_preserves_dimensions },
        { "LayoutSetObject3DPositionClampsLockedPrismToBounds", test_layout_set_object3d_position_clamps_locked_prism_to_bounds },
        { "LayoutRotateObject3DRotatesPlaneFrameAndPreservesDimensions", test_layout_rotate_object3d_rotates_plane_frame_and_preserves_dimensions },
        { "LayoutRotateObject3DBaselineCompositionIsDeterministic", test_layout_rotate_object3d_baseline_composition_is_deterministic },
        { "LayoutRotateObject3DClampsLockedBounds", test_layout_rotate_object3d_clamps_locked_bounds },
        { "UIPanelDisplayUnitConversionRoundtrip", test_ui_panel_display_unit_conversion_roundtrip },
        { "UIPanelDisplayUnitRoundingDeterminism", test_ui_panel_display_unit_rounding_determinism }
    };

    return run_test_cases("LayoutObject3DResize", cases, sizeof(cases) / sizeof(cases[0]));
}
