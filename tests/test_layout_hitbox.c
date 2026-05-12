#include "test_layout_internal.h"
#include "Editor/object_handle_gizmo.h"

static bool test_hitbox_plane_object_is_selectable(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    PlanePrimitiveCreateParams params;
    Layout_PlanePrimitiveCreateParams_SetDefaults(&params);
    params.width = 3.0f;
    params.height = 3.0f;
    params.lockToBounds = false;

    uint32_t objectId = 0u;
    bool adjusted = false;
    TEST_ASSERT(Layout_CreatePlanePrimitive(layout, &params, &objectId, &adjusted));
    const Object3D* object = Layout_ObjectStore_FindConst(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);

    Global_RebuildHitboxesIfDirty();
    Vec2 centerView = Vec3_ProjectToView(object->transform.position, state->activePlane, &state->freeViewCamera);
    Vec2 centerScreen = WorldToScreen(centerView, &state->grid);
    Hitbox hit = HitboxSystem_GetHitAt((int)centerScreen.x, (int)centerScreen.y);
    TEST_ASSERT(hit.type == HITBOX_OBJECT3D);
    TEST_ASSERT(hit.index == (int)objectId);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_hitbox_plane_resize_handles_emit_for_selected_plane(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    PlanePrimitiveCreateParams params;
    Layout_PlanePrimitiveCreateParams_SetDefaults(&params);
    params.width = 3.0f;
    params.height = 3.0f;
    params.lockToBounds = false;

    uint32_t objectId = 0u;
    TEST_ASSERT(Layout_CreatePlanePrimitive(layout, &params, &objectId, NULL));
    const Object3D* object = Layout_ObjectStore_FindConst(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);

    state->editor.selectedObject3DId = objectId;
    state->editor.selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    Global_FlagHitboxesDirty();
    Global_RebuildHitboxesIfDirty();

    Vec3 corners[4] = {0};
    TEST_ASSERT(Layout_Object3D_ComputePlaneCorners(object, corners));
    {
        Vec2 cornerView = Vec3_ProjectToView(corners[2], state->activePlane, &state->freeViewCamera);
        Vec2 cornerScreen = WorldToScreen(cornerView, &state->grid);
        Hitbox cornerHit = HitboxSystem_GetHitAt((int)cornerScreen.x, (int)cornerScreen.y);
        TEST_ASSERT(cornerHit.type == HITBOX_OBJECT3D_PLANE_CORNER);
        TEST_ASSERT(cornerHit.index == (int)objectId);
        TEST_ASSERT(cornerHit.subIndex == PLANE_RESIZE_HANDLE_CORNER_POS_U_POS_V);
    }
    {
        Vec3 edgeMid = Vec3_Scale(Vec3_Add(corners[1], corners[2]), 0.5f);
        Vec2 edgeView = Vec3_ProjectToView(edgeMid, state->activePlane, &state->freeViewCamera);
        Vec2 edgeScreen = WorldToScreen(edgeView, &state->grid);
        Hitbox edgeHit = HitboxSystem_GetHitAt((int)edgeScreen.x, (int)edgeScreen.y);
        TEST_ASSERT(edgeHit.type == HITBOX_OBJECT3D_PLANE_EDGE);
        TEST_ASSERT(edgeHit.index == (int)objectId);
        TEST_ASSERT(edgeHit.subIndex == PLANE_RESIZE_HANDLE_EDGE_POS_U);
    }

    ld_test_shutdown_runtime();
    return true;
}

static bool test_hitbox_plane_gizmo_axes_emit_for_selected_corner_in_free_view(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    PlanePrimitiveCreateParams params;
    Layout_PlanePrimitiveCreateParams_SetDefaults(&params);
    params.width = 4.0f;
    params.height = 3.0f;
    params.lockToBounds = false;

    uint32_t objectId = 0u;
    TEST_ASSERT(Layout_CreatePlanePrimitive(layout, &params, &objectId, NULL));
    const Object3D* object = Layout_ObjectStore_FindConst(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);

    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_3D, false));
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.yawDeg = 35.0f;
    state->freeViewCamera.pitchDeg = 20.0f;
    state->freeViewCamera.target = object->transform.position;

    state->editor.selectedObject3DId = objectId;
    state->editor.selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_CORNER_POS_U_POS_V;
    state->editor.selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    Global_FlagHitboxesDirty();
    Global_RebuildHitboxesIfDirty();

    Vec3 handleWorld = {0};
    TEST_ASSERT(Layout_PlaneResizeHandleWorldPoint(object,
                                                   PLANE_RESIZE_HANDLE_CORNER_POS_U_POS_V,
                                                   &handleWorld));
    const float axisWorldLen = fmaxf(layout->gridSize * 2.0f, 1.0f);

    {
        Vec3 axisU = Layout_PlaneAxisDirection_WorldVector(object, RECT_PRISM_AXIS_DIR_POS_U);
        Vec3 tip = Vec3_Add(handleWorld, Vec3_Scale(axisU, axisWorldLen));
        Vec2 tipView = Vec3_ProjectToView(tip, state->activePlane, &state->freeViewCamera);
        Vec2 tipScreen = WorldToScreen(tipView, &state->grid);
        Hitbox hit = HitboxSystem_GetHitAt((int)tipScreen.x, (int)tipScreen.y);
        TEST_ASSERT(hit.type == HITBOX_OBJECT3D_GIZMO_AXIS);
        TEST_ASSERT(hit.index == (int)objectId);
        TEST_ASSERT(hit.subIndex == RECT_PRISM_AXIS_DIR_POS_U);
    }
    {
        Vec3 axisV = Layout_PlaneAxisDirection_WorldVector(object, RECT_PRISM_AXIS_DIR_POS_V);
        Vec3 tip = Vec3_Add(handleWorld, Vec3_Scale(axisV, axisWorldLen));
        Vec2 tipView = Vec3_ProjectToView(tip, state->activePlane, &state->freeViewCamera);
        Vec2 tipScreen = WorldToScreen(tipView, &state->grid);
        Hitbox hit = HitboxSystem_GetHitAt((int)tipScreen.x, (int)tipScreen.y);
        TEST_ASSERT(hit.type == HITBOX_OBJECT3D_GIZMO_AXIS);
        TEST_ASSERT(hit.index == (int)objectId);
        TEST_ASSERT(hit.subIndex == RECT_PRISM_AXIS_DIR_POS_V);
    }

    ld_test_shutdown_runtime();
    return true;
}

static bool test_hitbox_plane_edge_gizmo_allows_only_resize_axis(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    PlanePrimitiveCreateParams params;
    Layout_PlanePrimitiveCreateParams_SetDefaults(&params);
    params.width = 4.0f;
    params.height = 3.0f;
    params.lockToBounds = false;

    uint32_t objectId = 0u;
    TEST_ASSERT(Layout_CreatePlanePrimitive(layout, &params, &objectId, NULL));
    const Object3D* object = Layout_ObjectStore_FindConst(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);

    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_3D, false));
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.yawDeg = 35.0f;
    state->freeViewCamera.pitchDeg = 20.0f;
    state->freeViewCamera.target = object->transform.position;

    state->editor.selectedObject3DId = objectId;
    state->editor.selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_EDGE_POS_U;
    state->editor.selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    Global_FlagHitboxesDirty();
    Global_RebuildHitboxesIfDirty();

    Vec3 handleWorld = {0};
    TEST_ASSERT(Layout_PlaneResizeHandleWorldPoint(object,
                                                   PLANE_RESIZE_HANDLE_EDGE_POS_U,
                                                   &handleWorld));
    const float axisWorldLen = fmaxf(layout->gridSize * 2.0f, 1.0f);

    {
        Vec3 axisU = Layout_PlaneAxisDirection_WorldVector(object, RECT_PRISM_AXIS_DIR_POS_U);
        Vec3 tip = Vec3_Add(handleWorld, Vec3_Scale(axisU, axisWorldLen));
        Vec2 tipView = Vec3_ProjectToView(tip, state->activePlane, &state->freeViewCamera);
        Vec2 tipScreen = WorldToScreen(tipView, &state->grid);
        Hitbox hit = HitboxSystem_GetHitAt((int)tipScreen.x, (int)tipScreen.y);
        TEST_ASSERT(hit.type == HITBOX_OBJECT3D_GIZMO_AXIS);
        TEST_ASSERT(hit.index == (int)objectId);
        TEST_ASSERT(hit.subIndex == RECT_PRISM_AXIS_DIR_POS_U);
    }
    {
        Vec3 axisV = Layout_PlaneAxisDirection_WorldVector(object, RECT_PRISM_AXIS_DIR_POS_V);
        Vec3 tip = Vec3_Add(handleWorld, Vec3_Scale(axisV, axisWorldLen));
        Vec2 tipView = Vec3_ProjectToView(tip, state->activePlane, &state->freeViewCamera);
        Vec2 tipScreen = WorldToScreen(tipView, &state->grid);
        Hitbox hit = HitboxSystem_GetHitAt((int)tipScreen.x, (int)tipScreen.y);
        TEST_ASSERT(!(hit.type == HITBOX_OBJECT3D_GIZMO_AXIS &&
                      hit.subIndex == RECT_PRISM_AXIS_DIR_POS_V));
    }

    ld_test_shutdown_runtime();
    return true;
}

static bool test_object_handle_gizmo_target_selection_normalizes_plane_and_prism(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    PlanePrimitiveCreateParams planeParams;
    Layout_PlanePrimitiveCreateParams_SetDefaults(&planeParams);
    planeParams.lockToBounds = false;

    uint32_t planeId = 0u;
    TEST_ASSERT(Layout_CreatePlanePrimitive(layout, &planeParams, &planeId, NULL));
    const Object3D* plane = Layout_ObjectStore_FindConst(&layout->objectStore, planeId);
    TEST_ASSERT(plane != NULL);

    ObjectHandleGizmoTarget planeTarget = ObjectHandleGizmoTarget_None();
    TEST_ASSERT(ObjectHandleGizmoTarget_FromSelection(plane,
                                                      planeId,
                                                      PLANE_RESIZE_HANDLE_EDGE_POS_U,
                                                      RECT_PRISM_RESIZE_HANDLE_NONE,
                                                      &planeTarget));
    TEST_ASSERT(planeTarget.kind == OBJECT_HANDLE_GIZMO_TARGET_PLANE_RESIZE);
    TEST_ASSERT(ObjectHandleGizmoTarget_AxisAllowed(&planeTarget, RECT_PRISM_AXIS_DIR_POS_U));
    TEST_ASSERT(!ObjectHandleGizmoTarget_AxisAllowed(&planeTarget, RECT_PRISM_AXIS_DIR_POS_V));
    TEST_ASSERT(!ObjectHandleGizmoTarget_AxisAllowed(&planeTarget, RECT_PRISM_AXIS_DIR_POS_N));

    RectPrismPrimitiveCreateParams prismParams;
    Layout_RectPrismPrimitiveCreateParams_SetDefaults(&prismParams);
    prismParams.lockToBounds = false;

    uint32_t prismId = 0u;
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(layout, &prismParams, &prismId, NULL));
    const Object3D* prism = Layout_ObjectStore_FindConst(&layout->objectStore, prismId);
    TEST_ASSERT(prism != NULL);

    ObjectHandleGizmoTarget prismTarget = ObjectHandleGizmoTarget_None();
    TEST_ASSERT(ObjectHandleGizmoTarget_FromSelection(prism,
                                                      prismId,
                                                      PLANE_RESIZE_HANDLE_NONE,
                                                      RECT_PRISM_RESIZE_HANDLE_EDGE_1,
                                                      &prismTarget));
    TEST_ASSERT(prismTarget.kind == OBJECT_HANDLE_GIZMO_TARGET_PRISM_RESIZE);
    TEST_ASSERT(ObjectHandleGizmoTarget_AxisAllowed(&prismTarget, RECT_PRISM_AXIS_DIR_POS_U));
    TEST_ASSERT(!ObjectHandleGizmoTarget_AxisAllowed(&prismTarget, RECT_PRISM_AXIS_DIR_POS_V));
    TEST_ASSERT(ObjectHandleGizmoTarget_AxisAllowed(&prismTarget, RECT_PRISM_AXIS_DIR_POS_N));

    ObjectHandleGizmoTarget invalidTarget = ObjectHandleGizmoTarget_None();
    TEST_ASSERT(!ObjectHandleGizmoTarget_FromSelection(plane,
                                                       planeId,
                                                       PLANE_RESIZE_HANDLE_NONE,
                                                       RECT_PRISM_RESIZE_HANDLE_EDGE_1,
                                                       &invalidTarget));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_hitbox_scene_bounds_face_handles_emit_in_free_view(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_3D, false));
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.yawDeg = 35.0f;
    state->freeViewCamera.pitchDeg = 20.0f;
    state->freeViewCamera.target = (Vec3){ 0.0f, 0.0f, 0.0f };

    layout->scene3d.bounds.enabled = true;
    layout->scene3d.bounds.min = (Vec3){ -2.0f, -3.0f, -4.0f };
    layout->scene3d.bounds.max = (Vec3){  2.0f,  3.0f,  4.0f };

    Global_FlagHitboxesDirty();
    Global_RebuildHitboxesIfDirty();

    Vec3 handleWorld = {0};
    TEST_ASSERT(Layout_SceneBoundsHandleWorldPoint(&layout->scene3d.bounds,
                                                   SCENE_BOUNDS_HANDLE_MAX_X,
                                                   &handleWorld));
    Vec2 handleView = Vec3_ProjectToView(handleWorld, state->activePlane, &state->freeViewCamera);
    Vec2 handleScreen = WorldToScreen(handleView, &state->grid);
    Hitbox hit = HitboxSystem_GetHitAt((int)handleScreen.x, (int)handleScreen.y);
    TEST_ASSERT(hit.type == HITBOX_SCENE_BOUNDS_HANDLE);
    TEST_ASSERT(hit.subIndex == SCENE_BOUNDS_HANDLE_MAX_X);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_hitbox_scene_bounds_handles_hidden_when_toggle_off(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_3D, false));
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.yawDeg = 35.0f;
    state->freeViewCamera.pitchDeg = 20.0f;
    state->freeViewCamera.target = (Vec3){ 0.0f, 0.0f, 0.0f };

    layout->scene3d.bounds.enabled = true;
    layout->scene3d.bounds.min = (Vec3){ -2.0f, -3.0f, -4.0f };
    layout->scene3d.bounds.max = (Vec3){  2.0f,  3.0f,  4.0f };
    state->editor.sceneBoundsHandlesVisible = false;
    state->editor.selectedSceneBoundsHandle = SCENE_BOUNDS_HANDLE_MAX_X;

    Global_FlagHitboxesDirty();
    Global_RebuildHitboxesIfDirty();

    Vec3 handleWorld = {0};
    TEST_ASSERT(Layout_SceneBoundsHandleWorldPoint(&layout->scene3d.bounds,
                                                   SCENE_BOUNDS_HANDLE_MAX_X,
                                                   &handleWorld));
    Vec2 handleView = Vec3_ProjectToView(handleWorld, state->activePlane, &state->freeViewCamera);
    Vec2 handleScreen = WorldToScreen(handleView, &state->grid);
    Hitbox hit = HitboxSystem_GetHitAt((int)handleScreen.x, (int)handleScreen.y);
    TEST_ASSERT(hit.type != HITBOX_SCENE_BOUNDS_HANDLE);
    TEST_ASSERT(hit.type != HITBOX_SCENE_BOUNDS_GIZMO_AXIS);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_hitbox_scene_bounds_selected_face_emits_one_axis_gizmo(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_3D, false));
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.yawDeg = 35.0f;
    state->freeViewCamera.pitchDeg = 20.0f;
    state->freeViewCamera.target = (Vec3){ 0.0f, 0.0f, 0.0f };

    layout->scene3d.bounds.enabled = true;
    layout->scene3d.bounds.min = (Vec3){ -2.0f, -3.0f, -4.0f };
    layout->scene3d.bounds.max = (Vec3){  2.0f,  3.0f,  4.0f };
    state->editor.selectedSceneBoundsHandle = SCENE_BOUNDS_HANDLE_MAX_X;

    Global_FlagHitboxesDirty();
    Global_RebuildHitboxesIfDirty();

    Vec3 handleWorld = {0};
    TEST_ASSERT(Layout_SceneBoundsHandleWorldPoint(&layout->scene3d.bounds,
                                                   SCENE_BOUNDS_HANDLE_MAX_X,
                                                   &handleWorld));
    const float axisWorldLen = fmaxf(layout->gridSize * 2.0f, 1.0f);
    Vec3 axisX = Layout_SceneBoundsAxisDirection_WorldVector(RECT_PRISM_AXIS_DIR_POS_U);
    Vec3 tip = Vec3_Add(handleWorld, Vec3_Scale(axisX, axisWorldLen));
    Vec2 tipView = Vec3_ProjectToView(tip, state->activePlane, &state->freeViewCamera);
    Vec2 tipScreen = WorldToScreen(tipView, &state->grid);
    Hitbox hit = HitboxSystem_GetHitAt((int)tipScreen.x, (int)tipScreen.y);
    TEST_ASSERT(hit.type == HITBOX_SCENE_BOUNDS_GIZMO_AXIS);
    TEST_ASSERT(hit.subIndex == RECT_PRISM_AXIS_DIR_POS_U);

    Vec3 axisY = Layout_SceneBoundsAxisDirection_WorldVector(RECT_PRISM_AXIS_DIR_POS_V);
    tip = Vec3_Add(handleWorld, Vec3_Scale(axisY, axisWorldLen));
    tipView = Vec3_ProjectToView(tip, state->activePlane, &state->freeViewCamera);
    tipScreen = WorldToScreen(tipView, &state->grid);
    hit = HitboxSystem_GetHitAt((int)tipScreen.x, (int)tipScreen.y);
    TEST_ASSERT(!(hit.type == HITBOX_SCENE_BOUNDS_GIZMO_AXIS &&
                  hit.subIndex == RECT_PRISM_AXIS_DIR_POS_V));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_hitbox_scene_bounds_edge_and_corner_handles_emit_in_free_view(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_3D, false));
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.yawDeg = 35.0f;
    state->freeViewCamera.pitchDeg = 20.0f;
    state->freeViewCamera.target = (Vec3){ 0.0f, 0.0f, 0.0f };

    layout->scene3d.bounds.enabled = true;
    layout->scene3d.bounds.min = (Vec3){ -2.0f, -3.0f, -4.0f };
    layout->scene3d.bounds.max = (Vec3){  2.0f,  3.0f,  4.0f };

    Global_FlagHitboxesDirty();
    Global_RebuildHitboxesIfDirty();

    Vec3 edgeWorld = {0};
    TEST_ASSERT(Layout_SceneBoundsHandleWorldPoint(&layout->scene3d.bounds,
                                                   SCENE_BOUNDS_HANDLE_EDGE_X_MIN_Y_MAX_Z,
                                                   &edgeWorld));
    Vec2 edgeView = Vec3_ProjectToView(edgeWorld, state->activePlane, &state->freeViewCamera);
    Vec2 edgeScreen = WorldToScreen(edgeView, &state->grid);
    Hitbox edgeHit = HitboxSystem_GetHitAt((int)edgeScreen.x, (int)edgeScreen.y);
    TEST_ASSERT(edgeHit.type == HITBOX_SCENE_BOUNDS_HANDLE);
    TEST_ASSERT(edgeHit.subIndex == SCENE_BOUNDS_HANDLE_EDGE_X_MIN_Y_MAX_Z);

    Vec3 cornerWorld = {0};
    TEST_ASSERT(Layout_SceneBoundsHandleWorldPoint(&layout->scene3d.bounds,
                                                   SCENE_BOUNDS_HANDLE_CORNER_MAX_X_MIN_Y_MAX_Z,
                                                   &cornerWorld));
    Vec2 cornerView = Vec3_ProjectToView(cornerWorld, state->activePlane, &state->freeViewCamera);
    Vec2 cornerScreen = WorldToScreen(cornerView, &state->grid);
    Hitbox cornerHit = HitboxSystem_GetHitAt((int)cornerScreen.x, (int)cornerScreen.y);
    TEST_ASSERT(cornerHit.type == HITBOX_SCENE_BOUNDS_HANDLE);
    TEST_ASSERT(cornerHit.subIndex == SCENE_BOUNDS_HANDLE_CORNER_MAX_X_MIN_Y_MAX_Z);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_hitbox_scene_bounds_edge_and_corner_gizmo_axis_masks(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_3D, false));
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.yawDeg = 35.0f;
    state->freeViewCamera.pitchDeg = 20.0f;
    state->freeViewCamera.target = (Vec3){ 0.0f, 0.0f, 0.0f };

    layout->scene3d.bounds.enabled = true;
    layout->scene3d.bounds.min = (Vec3){ -2.0f, -3.0f, -4.0f };
    layout->scene3d.bounds.max = (Vec3){  2.0f,  3.0f,  4.0f };

    state->editor.selectedSceneBoundsHandle = SCENE_BOUNDS_HANDLE_EDGE_X_MIN_Y_MAX_Z;
    Global_FlagHitboxesDirty();
    Global_RebuildHitboxesIfDirty();

    Vec3 edgeWorld = {0};
    TEST_ASSERT(Layout_SceneBoundsHandleWorldPoint(&layout->scene3d.bounds,
                                                   SCENE_BOUNDS_HANDLE_EDGE_X_MIN_Y_MAX_Z,
                                                   &edgeWorld));
    const float axisWorldLen = fmaxf(layout->gridSize * 2.0f, 1.0f);
    Vec3 axisY = Layout_SceneBoundsAxisDirection_WorldVector(RECT_PRISM_AXIS_DIR_POS_V);
    Vec3 tip = Vec3_Add(edgeWorld, Vec3_Scale(axisY, axisWorldLen));
    Vec2 tipView = Vec3_ProjectToView(tip, state->activePlane, &state->freeViewCamera);
    Vec2 tipScreen = WorldToScreen(tipView, &state->grid);
    Hitbox hit = HitboxSystem_GetHitAt((int)tipScreen.x, (int)tipScreen.y);
    TEST_ASSERT(hit.type == HITBOX_SCENE_BOUNDS_GIZMO_AXIS);
    TEST_ASSERT(hit.subIndex == RECT_PRISM_AXIS_DIR_POS_V);

    Vec3 axisX = Layout_SceneBoundsAxisDirection_WorldVector(RECT_PRISM_AXIS_DIR_POS_U);
    tip = Vec3_Add(edgeWorld, Vec3_Scale(axisX, axisWorldLen));
    tipView = Vec3_ProjectToView(tip, state->activePlane, &state->freeViewCamera);
    tipScreen = WorldToScreen(tipView, &state->grid);
    hit = HitboxSystem_GetHitAt((int)tipScreen.x, (int)tipScreen.y);
    TEST_ASSERT(!(hit.type == HITBOX_SCENE_BOUNDS_GIZMO_AXIS &&
                  hit.subIndex == RECT_PRISM_AXIS_DIR_POS_U));

    state->editor.selectedSceneBoundsHandle = SCENE_BOUNDS_HANDLE_CORNER_MAX_X_MIN_Y_MAX_Z;
    Global_FlagHitboxesDirty();
    Global_RebuildHitboxesIfDirty();

    Vec3 cornerWorld = {0};
    TEST_ASSERT(Layout_SceneBoundsHandleWorldPoint(&layout->scene3d.bounds,
                                                   SCENE_BOUNDS_HANDLE_CORNER_MAX_X_MIN_Y_MAX_Z,
                                                   &cornerWorld));
    axisX = Layout_SceneBoundsAxisDirection_WorldVector(RECT_PRISM_AXIS_DIR_POS_U);
    tip = Vec3_Add(cornerWorld, Vec3_Scale(axisX, axisWorldLen));
    tipView = Vec3_ProjectToView(tip, state->activePlane, &state->freeViewCamera);
    tipScreen = WorldToScreen(tipView, &state->grid);
    hit = HitboxSystem_GetHitAt((int)tipScreen.x, (int)tipScreen.y);
    TEST_ASSERT(hit.type == HITBOX_SCENE_BOUNDS_GIZMO_AXIS);
    TEST_ASSERT(hit.subIndex == RECT_PRISM_AXIS_DIR_POS_U);

    axisY = Layout_SceneBoundsAxisDirection_WorldVector(RECT_PRISM_AXIS_DIR_NEG_V);
    tip = Vec3_Add(cornerWorld, Vec3_Scale(axisY, axisWorldLen));
    tipView = Vec3_ProjectToView(tip, state->activePlane, &state->freeViewCamera);
    tipScreen = WorldToScreen(tipView, &state->grid);
    hit = HitboxSystem_GetHitAt((int)tipScreen.x, (int)tipScreen.y);
    TEST_ASSERT(hit.type == HITBOX_SCENE_BOUNDS_GIZMO_AXIS);
    TEST_ASSERT(hit.subIndex == RECT_PRISM_AXIS_DIR_NEG_V);

    Vec3 axisZ = Layout_SceneBoundsAxisDirection_WorldVector(RECT_PRISM_AXIS_DIR_POS_N);
    tip = Vec3_Add(cornerWorld, Vec3_Scale(axisZ, axisWorldLen));
    tipView = Vec3_ProjectToView(tip, state->activePlane, &state->freeViewCamera);
    tipScreen = WorldToScreen(tipView, &state->grid);
    hit = HitboxSystem_GetHitAt((int)tipScreen.x, (int)tipScreen.y);
    TEST_ASSERT(hit.type == HITBOX_SCENE_BOUNDS_GIZMO_AXIS);
    TEST_ASSERT(hit.subIndex == RECT_PRISM_AXIS_DIR_POS_N);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_hitbox_scene_bounds_center_handle_emits_translate_gizmo(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_3D, false));
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.yawDeg = 35.0f;
    state->freeViewCamera.pitchDeg = 20.0f;
    state->freeViewCamera.target = (Vec3){ 0.0f, 0.0f, 0.0f };

    layout->scene3d.bounds.enabled = true;
    layout->scene3d.bounds.min = (Vec3){ -2.0f, -3.0f, -4.0f };
    layout->scene3d.bounds.max = (Vec3){  2.0f,  3.0f,  4.0f };

    Global_FlagHitboxesDirty();
    Global_RebuildHitboxesIfDirty();

    Vec3 centerWorld = {0};
    TEST_ASSERT(Layout_SceneBoundsHandleWorldPoint(&layout->scene3d.bounds,
                                                   SCENE_BOUNDS_HANDLE_CENTER,
                                                   &centerWorld));
    Vec2 centerView = Vec3_ProjectToView(centerWorld, state->activePlane, &state->freeViewCamera);
    Vec2 centerScreen = WorldToScreen(centerView, &state->grid);
    Hitbox hit = HitboxSystem_GetHitAt((int)centerScreen.x, (int)centerScreen.y);
    TEST_ASSERT(hit.type == HITBOX_SCENE_BOUNDS_HANDLE);
    TEST_ASSERT(hit.subIndex == SCENE_BOUNDS_HANDLE_CENTER);

    state->editor.selectedSceneBoundsHandle = SCENE_BOUNDS_HANDLE_CENTER;
    Global_FlagHitboxesDirty();
    Global_RebuildHitboxesIfDirty();

    const float axisWorldLen = fmaxf(layout->gridSize * 2.0f, 1.0f);
    Vec3 axisX = Layout_SceneBoundsAxisDirection_WorldVector(RECT_PRISM_AXIS_DIR_POS_U);
    Vec3 tip = Vec3_Add(centerWorld, Vec3_Scale(axisX, axisWorldLen));
    Vec2 tipView = Vec3_ProjectToView(tip, state->activePlane, &state->freeViewCamera);
    Vec2 tipScreen = WorldToScreen(tipView, &state->grid);
    hit = HitboxSystem_GetHitAt((int)tipScreen.x, (int)tipScreen.y);
    TEST_ASSERT(hit.type == HITBOX_SCENE_BOUNDS_GIZMO_AXIS);
    TEST_ASSERT(hit.subIndex == RECT_PRISM_AXIS_DIR_POS_U);

    Vec3 axisY = Layout_SceneBoundsAxisDirection_WorldVector(RECT_PRISM_AXIS_DIR_NEG_V);
    tip = Vec3_Add(centerWorld, Vec3_Scale(axisY, axisWorldLen));
    tipView = Vec3_ProjectToView(tip, state->activePlane, &state->freeViewCamera);
    tipScreen = WorldToScreen(tipView, &state->grid);
    hit = HitboxSystem_GetHitAt((int)tipScreen.x, (int)tipScreen.y);
    TEST_ASSERT(hit.type == HITBOX_SCENE_BOUNDS_GIZMO_AXIS);
    TEST_ASSERT(hit.subIndex == RECT_PRISM_AXIS_DIR_NEG_V);

    Vec3 axisZ = Layout_SceneBoundsAxisDirection_WorldVector(RECT_PRISM_AXIS_DIR_POS_N);
    tip = Vec3_Add(centerWorld, Vec3_Scale(axisZ, axisWorldLen));
    tipView = Vec3_ProjectToView(tip, state->activePlane, &state->freeViewCamera);
    tipScreen = WorldToScreen(tipView, &state->grid);
    hit = HitboxSystem_GetHitAt((int)tipScreen.x, (int)tipScreen.y);
    TEST_ASSERT(hit.type == HITBOX_SCENE_BOUNDS_GIZMO_AXIS);
    TEST_ASSERT(hit.subIndex == RECT_PRISM_AXIS_DIR_POS_N);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_hitbox_rect_prism_object_is_selectable(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    RectPrismPrimitiveCreateParams params;
    Layout_RectPrismPrimitiveCreateParams_SetDefaults(&params);
    params.width = 3.0f;
    params.height = 2.0f;
    params.depth = 1.0f;
    params.lockToBounds = false;

    uint32_t objectId = 0u;
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(layout, &params, &objectId, NULL));
    const Object3D* object = Layout_ObjectStore_FindConst(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);

    Global_RebuildHitboxesIfDirty();
    Vec2 centerView = Vec3_ProjectToView(object->transform.position, state->activePlane, &state->freeViewCamera);
    Vec2 centerScreen = WorldToScreen(centerView, &state->grid);
    Hitbox hit = HitboxSystem_GetHitAt((int)centerScreen.x, (int)centerScreen.y);
    TEST_ASSERT(hit.type == HITBOX_OBJECT3D);
    TEST_ASSERT(hit.index == (int)objectId);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_hitbox_rect_prism_resize_handles_emit_for_selected_prism(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    RectPrismPrimitiveCreateParams params;
    Layout_RectPrismPrimitiveCreateParams_SetDefaults(&params);
    params.width = 3.0f;
    params.height = 3.0f;
    params.depth = 2.0f;
    params.lockToBounds = false;

    uint32_t objectId = 0u;
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(layout, &params, &objectId, NULL));
    const Object3D* object = Layout_ObjectStore_FindConst(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);

    state->editor.selectedObject3DId = objectId;
    state->editor.selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    Global_FlagHitboxesDirty();
    Global_RebuildHitboxesIfDirty();

    Vec3 corners[8] = {0};
    TEST_ASSERT(Layout_Object3D_ComputeRectPrismCorners(object, corners));
    Vec3 bottomCenter = Vec3_Scale(Vec3_Add(Vec3_Add(corners[0], corners[1]),
                                            Vec3_Add(corners[2], corners[3])), 0.25f);
    Vec3 topCenter = Vec3_Scale(Vec3_Add(Vec3_Add(corners[4], corners[5]),
                                         Vec3_Add(corners[6], corners[7])), 0.25f);
    const bool useTopFace =
        ViewPlane_AbsDistance(state->activePlane, topCenter) <= ViewPlane_AbsDistance(state->activePlane, bottomCenter);
    const int baseIndex = useTopFace ? 4 : 0;
    Vec3 faceCorners[4] = {
        corners[baseIndex + 0],
        corners[baseIndex + 1],
        corners[baseIndex + 2],
        corners[baseIndex + 3]
    };

    {
        Vec2 cornerView = Vec3_ProjectToView(faceCorners[2], state->activePlane, &state->freeViewCamera);
        Vec2 cornerScreen = WorldToScreen(cornerView, &state->grid);
        Hitbox cornerHit = HitboxSystem_GetHitAt((int)cornerScreen.x, (int)cornerScreen.y);
        TEST_ASSERT(cornerHit.type == HITBOX_OBJECT3D_PLANE_CORNER);
        TEST_ASSERT(cornerHit.index == (int)objectId);
        TEST_ASSERT(cornerHit.subIndex == PLANE_RESIZE_HANDLE_CORNER_POS_U_POS_V);
    }
    {
        Vec3 edgeMid = Vec3_Scale(Vec3_Add(faceCorners[1], faceCorners[2]), 0.5f);
        Vec2 edgeView = Vec3_ProjectToView(edgeMid, state->activePlane, &state->freeViewCamera);
        Vec2 edgeScreen = WorldToScreen(edgeView, &state->grid);
        Hitbox edgeHit = HitboxSystem_GetHitAt((int)edgeScreen.x, (int)edgeScreen.y);
        TEST_ASSERT(edgeHit.type == HITBOX_OBJECT3D_PLANE_EDGE);
        TEST_ASSERT(edgeHit.index == (int)objectId);
        TEST_ASSERT(edgeHit.subIndex == PLANE_RESIZE_HANDLE_EDGE_POS_U);
    }

    ld_test_shutdown_runtime();
    return true;
}

static bool test_hitbox_rect_prism_gizmo_axes_emit_for_selected_handle_in_free_view(void) {
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
    const Object3D* object = Layout_ObjectStore_FindConst(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);

    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_3D, false));
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.yawDeg = 35.0f;
    state->freeViewCamera.pitchDeg = 20.0f;
    state->freeViewCamera.target = object->transform.position;

    state->editor.selectedObject3DId = objectId;
    state->editor.selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    state->editor.selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_EDGE_1;
    Global_FlagHitboxesDirty();
    Global_RebuildHitboxesIfDirty();

    Vec3 handleWorld = {0};
    TEST_ASSERT(Layout_RectPrismResizeHandleWorldPoint(object,
                                                       RECT_PRISM_RESIZE_HANDLE_EDGE_1,
                                                       &handleWorld));
    const float axisWorldLen = fmaxf(layout->gridSize * 2.0f, 1.0f);

    {
        Vec3 axisU = Layout_RectPrismAxisDirection_WorldVector(object, RECT_PRISM_AXIS_DIR_POS_U);
        Vec3 tip = Vec3_Add(handleWorld, Vec3_Scale(axisU, axisWorldLen));
        Vec2 tipView = Vec3_ProjectToView(tip, state->activePlane, &state->freeViewCamera);
        Vec2 tipScreen = WorldToScreen(tipView, &state->grid);
        Hitbox hit = HitboxSystem_GetHitAt((int)tipScreen.x, (int)tipScreen.y);
        TEST_ASSERT(hit.type == HITBOX_OBJECT3D_GIZMO_AXIS);
        TEST_ASSERT(hit.index == (int)objectId);
        TEST_ASSERT(hit.subIndex == RECT_PRISM_AXIS_DIR_POS_U);
    }
    {
        Vec3 axisN = Layout_RectPrismAxisDirection_WorldVector(object, RECT_PRISM_AXIS_DIR_POS_N);
        Vec3 tip = Vec3_Add(handleWorld, Vec3_Scale(axisN, axisWorldLen));
        Vec2 tipView = Vec3_ProjectToView(tip, state->activePlane, &state->freeViewCamera);
        Vec2 tipScreen = WorldToScreen(tipView, &state->grid);
        Hitbox hit = HitboxSystem_GetHitAt((int)tipScreen.x, (int)tipScreen.y);
        TEST_ASSERT(hit.type == HITBOX_OBJECT3D_GIZMO_AXIS);
        TEST_ASSERT(hit.index == (int)objectId);
        TEST_ASSERT(hit.subIndex == RECT_PRISM_AXIS_DIR_POS_N);
    }
    {
        Vec3 axisV = Layout_RectPrismAxisDirection_WorldVector(object, RECT_PRISM_AXIS_DIR_POS_V);
        Vec3 tip = Vec3_Add(handleWorld, Vec3_Scale(axisV, axisWorldLen));
        Vec2 tipView = Vec3_ProjectToView(tip, state->activePlane, &state->freeViewCamera);
        Vec2 tipScreen = WorldToScreen(tipView, &state->grid);
        Hitbox hit = HitboxSystem_GetHitAt((int)tipScreen.x, (int)tipScreen.y);
        TEST_ASSERT(!(hit.type == HITBOX_OBJECT3D_GIZMO_AXIS &&
                      hit.subIndex == RECT_PRISM_AXIS_DIR_POS_V));
    }

    ld_test_shutdown_runtime();
    return true;
}

static bool test_hitbox_object3d_center_gizmo_axes_emit_for_selected_object_in_free_view(void) {
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
    const Object3D* object = Layout_ObjectStore_FindConst(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);

    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_3D, false));
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.yawDeg = 35.0f;
    state->freeViewCamera.pitchDeg = 20.0f;
    state->freeViewCamera.target = object->transform.position;

    state->editor.selectedObject3DId = objectId;
    state->editor.selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    state->editor.selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    Global_FlagHitboxesDirty();
    Global_RebuildHitboxesIfDirty();

    const float axisWorldLen = fmaxf(layout->gridSize * 2.0f, 1.0f);
    Vec3 tip = Vec3_Add(object->transform.position,
                        Vec3_Scale(GizmoAxisDirection_WorldVector(GIZMO_AXIS_DIR_POS_X), axisWorldLen));
    Vec2 tipView = Vec3_ProjectToView(tip, state->activePlane, &state->freeViewCamera);
    Vec2 tipScreen = WorldToScreen(tipView, &state->grid);
    Hitbox hit = HitboxSystem_GetHitAt((int)tipScreen.x, (int)tipScreen.y);
    TEST_ASSERT(hit.type == HITBOX_OBJECT3D_GIZMO_AXIS);
    TEST_ASSERT(hit.index == (int)objectId);
    TEST_ASSERT(hit.subIndex == GIZMO_AXIS_DIR_POS_X);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_hitbox_object3d_center_gizmo_hidden_when_prism_handle_selected(void) {
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
    const Object3D* object = Layout_ObjectStore_FindConst(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);

    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_3D, false));
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.yawDeg = 35.0f;
    state->freeViewCamera.pitchDeg = 20.0f;
    state->freeViewCamera.target = object->transform.position;

    state->editor.selectedObject3DId = objectId;
    state->editor.selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    state->editor.selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_EDGE_1;
    Global_FlagHitboxesDirty();
    Global_RebuildHitboxesIfDirty();

    const float axisWorldLen = fmaxf(layout->gridSize * 2.0f, 1.0f);
    Vec3 tip = Vec3_Add(object->transform.position,
                        Vec3_Scale(GizmoAxisDirection_WorldVector(GIZMO_AXIS_DIR_POS_X), axisWorldLen));
    Vec2 tipView = Vec3_ProjectToView(tip, state->activePlane, &state->freeViewCamera);
    Vec2 tipScreen = WorldToScreen(tipView, &state->grid);
    Hitbox hit = HitboxSystem_GetHitAt((int)tipScreen.x, (int)tipScreen.y);
    TEST_ASSERT(!(hit.type == HITBOX_OBJECT3D_GIZMO_AXIS && hit.subIndex == GIZMO_AXIS_DIR_POS_X));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_hitbox_rect_prism_free_view_emits_all_corner_and_edge_handles(void) {
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
    const Object3D* object = Layout_ObjectStore_FindConst(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);

    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_3D, false));
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.yawDeg = 35.0f;
    state->freeViewCamera.pitchDeg = 20.0f;
    state->freeViewCamera.target = object->transform.position;

    state->editor.selectedObject3DId = objectId;
    state->editor.selectedObject3DResizeHandle = PLANE_RESIZE_HANDLE_NONE;
    state->editor.selectedObject3DPrismHandle = RECT_PRISM_RESIZE_HANDLE_NONE;
    Global_FlagHitboxesDirty();
    Global_RebuildHitboxesIfDirty();

    for (int handle = RECT_PRISM_RESIZE_HANDLE_CORNER_0;
         handle <= RECT_PRISM_RESIZE_HANDLE_EDGE_11;
         ++handle) {
        Vec3 world = {0};
        TEST_ASSERT(Layout_RectPrismResizeHandleWorldPoint(object,
                                                           (RectPrismResizeHandleKind)handle,
                                                           &world));
        Vec2 view = Vec3_ProjectToView(world, state->activePlane, &state->freeViewCamera);
        Vec2 screen = WorldToScreen(view, &state->grid);
        Hitbox hit = HitboxSystem_GetHitAt((int)screen.x, (int)screen.y);
        TEST_ASSERT(hit.type == HITBOX_OBJECT3D_PRISM_HANDLE);
        TEST_ASSERT(hit.index == (int)objectId);
        TEST_ASSERT(hit.subIndex == handle);
    }

    ld_test_shutdown_runtime();
    return true;
}

static bool test_rect_prism_3d_handle_resize_allows_zero_depth_and_reexpand(void) {
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
    Object3D* object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);

    Vec3 axisU = Vec3_Normalize(object->rectPrism.frame.axisU);
    Vec3 axisV = Vec3_Normalize(object->rectPrism.frame.axisV);
    Vec3 axisN = Vec3_Normalize(object->rectPrism.frame.normal);
    Vec3 center = object->transform.position;
    const float halfW = object->rectPrism.width * 0.5f;
    const float halfH = object->rectPrism.height * 0.5f;
    const float halfD = object->rectPrism.depth * 0.5f;

    Vec3 collapsePoint = Vec3_Add(center,
                                  Vec3_Add(Vec3_Scale(axisU, halfW),
                                           Vec3_Add(Vec3_Scale(axisV, halfH),
                                                    Vec3_Scale(axisN, -halfD))));
    TEST_ASSERT(Layout_ResizeRectPrismFrom3DHandle(layout,
                                                   objectId,
                                                   RECT_PRISM_RESIZE_HANDLE_CORNER_6,
                                                   collapsePoint,
                                                   NULL));

    object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(Layout_ObjectStore_ValidateObject(object));
    TEST_ASSERT(ld_test_nearly_equal(object->rectPrism.depth, 0.0f));

    axisU = Vec3_Normalize(object->rectPrism.frame.axisU);
    axisV = Vec3_Normalize(object->rectPrism.frame.axisV);
    axisN = Vec3_Normalize(object->rectPrism.frame.normal);
    center = object->transform.position;
    Vec3 expandPoint = Vec3_Add(center,
                                Vec3_Add(Vec3_Scale(axisU, halfW),
                                         Vec3_Add(Vec3_Scale(axisV, halfH),
                                                  Vec3_Scale(axisN, 3.0f))));
    TEST_ASSERT(Layout_ResizeRectPrismFrom3DHandle(layout,
                                                   objectId,
                                                   RECT_PRISM_RESIZE_HANDLE_CORNER_6,
                                                   expandPoint,
                                                   NULL));

    object = Layout_ObjectStore_Find(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(Layout_ObjectStore_ValidateObject(object));
    TEST_ASSERT(ld_test_nearly_equal(object->rectPrism.depth, 3.0f));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_hitbox_prefers_nearer_plane_depth_for_overlapping_points(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    int nearIdx = Layout_AddAnchor3(layout, (Vec3){ 1.0f, 1.0f, 0.0f });
    int farIdx = Layout_AddAnchor3(layout, (Vec3){ 1.0f, 1.0f, 5.0f });
    TEST_ASSERT(nearIdx >= 0);
    TEST_ASSERT(farIdx >= 0);

    Global_RebuildHitboxesIfDirty();

    Vec2 screen = WorldToScreen((Vec2){ 1.0f, 1.0f }, &state->grid);
    Hitbox hit = HitboxSystem_GetHitAt((int)screen.x, (int)screen.y);

    TEST_ASSERT(hit.type == HITBOX_POINT);
    TEST_ASSERT(hit.index == nearIdx);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_hitbox_gizmo_axis_emits_for_selected_anchor_in_free_view(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    int anchorIdx = Layout_AddAnchor3(layout, (Vec3){ 0.0f, 0.0f, 0.0f });
    TEST_ASSERT(anchorIdx >= 0);
    Editor_SelectAnchor(&state->editor, anchorIdx, false);

    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_3D, false));
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.yawDeg = 90.0f;
    state->freeViewCamera.pitchDeg = 0.0f;
    state->freeViewCamera.target = (Vec3){ 0.0f, 0.0f, 0.0f };

    Global_RebuildHitboxesIfDirty();

    float axisWorldLen = fmaxf(layout->gridSize * 2.0f, 1.0f);
    Vec3 tip = Vec3_Add(layout->anchors[anchorIdx].pos, (Vec3){ axisWorldLen, 0.0f, 0.0f });
    Vec2 tipView = Vec3_ProjectToView(tip, state->activePlane, &state->freeViewCamera);
    Vec2 tipScreen = WorldToScreen(tipView, &state->grid);

    Hitbox hit = HitboxSystem_GetHitAt((int)tipScreen.x, (int)tipScreen.y);
    TEST_ASSERT(hit.type == HITBOX_GIZMO_AXIS);
    TEST_ASSERT(hit.index == anchorIdx);
    TEST_ASSERT(hit.subIndex == GIZMO_AXIS_DIR_POS_X);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_hitbox_gizmo_axis_disabled_when_free_view_off(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    int anchorIdx = Layout_AddAnchor3(layout, (Vec3){ 0.0f, 0.0f, 0.0f });
    TEST_ASSERT(anchorIdx >= 0);
    Editor_SelectAnchor(&state->editor, anchorIdx, false);

    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_3D, false));
    state->freeViewCamera.enabled = false;

    Global_RebuildHitboxesIfDirty();

    float axisWorldLen = fmaxf(layout->gridSize * 2.0f, 1.0f);
    Vec3 tip = Vec3_Add(layout->anchors[anchorIdx].pos, (Vec3){ axisWorldLen, 0.0f, 0.0f });
    Vec2 tipView = Vec3_ProjectToView(tip, state->activePlane, &state->freeViewCamera);
    Vec2 tipScreen = WorldToScreen(tipView, &state->grid);

    Hitbox hit = HitboxSystem_GetHitAt((int)tipScreen.x, (int)tipScreen.y);
    TEST_ASSERT(hit.type != HITBOX_GIZMO_AXIS);

    ld_test_shutdown_runtime();
    return true;
}

bool test_layout_hitbox_run_tests(void) {
    const TestCase cases[] = {
        { "HitboxPlaneObjectIsSelectable", test_hitbox_plane_object_is_selectable },
        { "HitboxPlaneResizeHandlesEmitForSelectedPlane", test_hitbox_plane_resize_handles_emit_for_selected_plane },
        { "HitboxPlaneGizmoAxesEmitForSelectedCornerInFreeView",
          test_hitbox_plane_gizmo_axes_emit_for_selected_corner_in_free_view },
        { "HitboxPlaneEdgeGizmoAllowsOnlyResizeAxis",
          test_hitbox_plane_edge_gizmo_allows_only_resize_axis },
        { "ObjectHandleGizmoTargetSelectionNormalizesPlaneAndPrism",
          test_object_handle_gizmo_target_selection_normalizes_plane_and_prism },
        { "HitboxSceneBoundsFaceHandlesEmitInFreeView",
          test_hitbox_scene_bounds_face_handles_emit_in_free_view },
        { "HitboxSceneBoundsHandlesHiddenWhenToggleOff",
          test_hitbox_scene_bounds_handles_hidden_when_toggle_off },
        { "HitboxSceneBoundsSelectedFaceEmitsOneAxisGizmo",
          test_hitbox_scene_bounds_selected_face_emits_one_axis_gizmo },
        { "HitboxSceneBoundsEdgeAndCornerHandlesEmitInFreeView",
          test_hitbox_scene_bounds_edge_and_corner_handles_emit_in_free_view },
        { "HitboxSceneBoundsEdgeAndCornerGizmoAxisMasks",
          test_hitbox_scene_bounds_edge_and_corner_gizmo_axis_masks },
        { "HitboxSceneBoundsCenterHandleEmitsTranslateGizmo",
          test_hitbox_scene_bounds_center_handle_emits_translate_gizmo },
        { "HitboxRectPrismObjectIsSelectable", test_hitbox_rect_prism_object_is_selectable },
        { "HitboxRectPrismResizeHandlesEmitForSelectedPrism", test_hitbox_rect_prism_resize_handles_emit_for_selected_prism },
        { "HitboxRectPrismGizmoAxesEmitForSelectedHandleInFreeView",
          test_hitbox_rect_prism_gizmo_axes_emit_for_selected_handle_in_free_view },
        { "HitboxObject3DCenterGizmoAxesEmitForSelectedObjectInFreeView",
          test_hitbox_object3d_center_gizmo_axes_emit_for_selected_object_in_free_view },
        { "HitboxObject3DCenterGizmoHiddenWhenPrismHandleSelected",
          test_hitbox_object3d_center_gizmo_hidden_when_prism_handle_selected },
        { "HitboxRectPrismFreeViewEmitsAllCornerAndEdgeHandles",
          test_hitbox_rect_prism_free_view_emits_all_corner_and_edge_handles },
        { "RectPrism3DHandleResizeAllowsZeroDepthAndReexpand",
          test_rect_prism_3d_handle_resize_allows_zero_depth_and_reexpand },
        { "HitboxPrefersNearerPlaneDepthForOverlappingPoints",
          test_hitbox_prefers_nearer_plane_depth_for_overlapping_points },
        { "HitboxGizmoAxisEmitsForSelectedAnchorInFreeView",
          test_hitbox_gizmo_axis_emits_for_selected_anchor_in_free_view },
        { "HitboxGizmoAxisDisabledWhenFreeViewOff",
          test_hitbox_gizmo_axis_disabled_when_free_view_off }
    };

    return run_test_cases("LayoutHitbox", cases, sizeof(cases) / sizeof(cases[0]));
}
