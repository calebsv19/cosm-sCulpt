#include "test_layout_internal.h"

static bool test_object_store_id_stability_and_tombstone_delete(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;
    LayoutObjectStore* store = &layout->objectStore;

    uint32_t id1 = Layout_ObjectStore_Create(store,
                                             OBJECT3D_KIND_PLANE,
                                             NULL,
                                             NULL,
                                             CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED,
                                             CORE_OBJECT_PLANE_XY);
    uint32_t id2 = Layout_ObjectStore_Create(store,
                                             OBJECT3D_KIND_RECT_PRISM,
                                             NULL,
                                             NULL,
                                             CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D,
                                             CORE_OBJECT_PLANE_XY);
    TEST_ASSERT(id1 == 1u);
    TEST_ASSERT(id2 == 2u);
    TEST_ASSERT(store->count == 2u);
    TEST_ASSERT(Layout_ObjectStore_LiveCount(store) == 2u);
    TEST_ASSERT(Layout_ObjectStore_Delete(store, id1));
    TEST_ASSERT(Layout_ObjectStore_Find(store, id1) == NULL);
    TEST_ASSERT(Layout_ObjectStore_LiveCount(store) == 1u);

    uint32_t id3 = Layout_ObjectStore_Create(store,
                                             OBJECT3D_KIND_PLANE,
                                             NULL,
                                             NULL,
                                             CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED,
                                             CORE_OBJECT_PLANE_XY);
    TEST_ASSERT(id3 == 3u);
    TEST_ASSERT(store->count == 3u);
    TEST_ASSERT(Layout_ObjectStore_LiveCount(store) == 2u);
    TEST_ASSERT(Layout_ObjectStore_Find(store, id2) != NULL);
    TEST_ASSERT(Layout_ObjectStore_Find(store, id3) != NULL);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_object_store_plane_lock_dimensional_rules_enforced(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    LayoutObjectStore* store = &state->layout.objectStore;
    Transform3D transform = Layout_Transform3D_Default();
    transform.position = (Vec3){ 4.0f, 2.0f, 1.0f };

    uint32_t id = Layout_ObjectStore_Create(store,
                                            OBJECT3D_KIND_PLANE,
                                            &transform,
                                            "plane_test",
                                            CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED,
                                            CORE_OBJECT_PLANE_YZ);
    TEST_ASSERT(id > 0u);
    Object3D* object = Layout_ObjectStore_Find(store, id);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(Layout_ObjectStore_ValidateObject(object));
    TEST_ASSERT(object->coreMeta.dimensional_mode == CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED);
    TEST_ASSERT(object->coreMeta.locked_plane == CORE_OBJECT_PLANE_YZ);
    TEST_ASSERT(ld_test_nearly_equal(object->transform.position.x, 0.0f));
    TEST_ASSERT(ld_test_nearly_equal(object->transform.position.y, 2.0f));
    TEST_ASSERT(ld_test_nearly_equal(object->transform.position.z, 1.0f));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_object_store_rejects_invalid_transform_scale(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    LayoutObjectStore* store = &state->layout.objectStore;
    Transform3D bad = Layout_Transform3D_Default();
    bad.scale.x = 0.0f;

    uint32_t id = Layout_ObjectStore_Create(store,
                                            OBJECT3D_KIND_RECT_PRISM,
                                            &bad,
                                            "prism_test",
                                            CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D,
                                            CORE_OBJECT_PLANE_XY);
    TEST_ASSERT(id == 0u);
    TEST_ASSERT(store->count == 0u);
    TEST_ASSERT(Layout_ObjectStore_LiveCount(store) == 0u);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_plane_primitive_creation_respects_bounds_and_construction_plane(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    layout->scene3d.bounds.enabled = true;
    layout->scene3d.bounds.min = (Vec3){ -2.0f, -2.0f, -2.0f };
    layout->scene3d.bounds.max = (Vec3){ 2.0f, 2.0f, 2.0f };
    layout->scene3d.constructionPlane.mode = CONSTRUCTION_PLANE_MODE_AXIS_ALIGNED;
    layout->scene3d.constructionPlane.axisAligned = (ViewPlane){ .axis = VIEW_PLANE_XY, .offset = 3.0f };

    PlanePrimitiveCreateParams params;
    Layout_PlanePrimitiveCreateParams_SetDefaults(&params);
    params.width = 10.0f;
    params.height = 10.0f;
    params.lockToBounds = true;

    uint32_t objectId = 0u;
    bool boundsAdjusted = false;
    TEST_ASSERT(Layout_CreatePlanePrimitive(layout, &params, &objectId, &boundsAdjusted));
    TEST_ASSERT(objectId > 0u);
    TEST_ASSERT(boundsAdjusted);

    const Object3D* object = Layout_ObjectStore_FindConst(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(object->kind == OBJECT3D_KIND_PLANE);
    TEST_ASSERT(ld_test_nearly_equal(object->transform.position.x, 0.0f));
    TEST_ASSERT(ld_test_nearly_equal(object->transform.position.y, 0.0f));
    TEST_ASSERT(ld_test_nearly_equal(object->transform.position.z, 2.0f));
    TEST_ASSERT(ld_test_nearly_equal(object->plane.width, 4.0f));
    TEST_ASSERT(ld_test_nearly_equal(object->plane.height, 4.0f));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_rect_prism_primitive_creation_respects_bounds_and_construction_plane(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    layout->scene3d.bounds.enabled = true;
    layout->scene3d.bounds.min = (Vec3){ -2.0f, -2.0f, -2.0f };
    layout->scene3d.bounds.max = (Vec3){ 2.0f, 2.0f, 2.0f };
    layout->scene3d.constructionPlane.mode = CONSTRUCTION_PLANE_MODE_AXIS_ALIGNED;
    layout->scene3d.constructionPlane.axisAligned = (ViewPlane){ .axis = VIEW_PLANE_XY, .offset = 0.0f };

    RectPrismPrimitiveCreateParams params;
    Layout_RectPrismPrimitiveCreateParams_SetDefaults(&params);
    params.width = 10.0f;
    params.height = 10.0f;
    params.depth = 10.0f;
    params.lockToBounds = true;

    uint32_t objectId = 0u;
    bool boundsAdjusted = false;
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(layout, &params, &objectId, &boundsAdjusted));
    TEST_ASSERT(objectId > 0u);
    TEST_ASSERT(boundsAdjusted);

    const Object3D* object = Layout_ObjectStore_FindConst(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(object->kind == OBJECT3D_KIND_RECT_PRISM);
    TEST_ASSERT(ld_test_nearly_equal(object->transform.position.x, 0.0f));
    TEST_ASSERT(ld_test_nearly_equal(object->transform.position.y, 0.0f));
    TEST_ASSERT(ld_test_nearly_equal(object->transform.position.z, 0.0f));
    TEST_ASSERT(ld_test_nearly_equal(object->rectPrism.width, 4.0f));
    TEST_ASSERT(ld_test_nearly_equal(object->rectPrism.height, 4.0f));
    TEST_ASSERT(ld_test_nearly_equal(object->rectPrism.depth, 4.0f));
    TEST_ASSERT(ld_test_vec3_nearly_equal(object->rectPrism.frame.origin, object->transform.position));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_rect_prism_primitive_creation_clamps_depth_to_zero_at_bounds_limit(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    layout->scene3d.bounds.enabled = true;
    layout->scene3d.bounds.min = (Vec3){ -2.0f, -2.0f, -2.0f };
    layout->scene3d.bounds.max = (Vec3){ 2.0f, 2.0f, 2.0f };
    layout->scene3d.constructionPlane.mode = CONSTRUCTION_PLANE_MODE_AXIS_ALIGNED;
    layout->scene3d.constructionPlane.axisAligned = (ViewPlane){ .axis = VIEW_PLANE_XY, .offset = 2.0f };

    RectPrismPrimitiveCreateParams params;
    Layout_RectPrismPrimitiveCreateParams_SetDefaults(&params);
    params.width = 1.0f;
    params.height = 1.0f;
    params.depth = 1.0f;
    params.lockToBounds = true;

    uint32_t objectId = 0u;
    bool boundsAdjusted = false;
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(layout, &params, &objectId, &boundsAdjusted));
    TEST_ASSERT(objectId > 0u);
    TEST_ASSERT(boundsAdjusted);
    const Object3D* object = Layout_ObjectStore_FindConst(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(ld_test_nearly_equal(object->rectPrism.width, 1.0f));
    TEST_ASSERT(ld_test_nearly_equal(object->rectPrism.height, 1.0f));
    TEST_ASSERT(ld_test_nearly_equal(object->rectPrism.depth, 0.0f));
    TEST_ASSERT(ld_test_nearly_equal(object->transform.position.z, 2.0f));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_object3d_compute_rect_prism_corners_contract(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    RectPrismPrimitiveCreateParams params;
    Layout_RectPrismPrimitiveCreateParams_SetDefaults(&params);
    params.width = 2.0f;
    params.height = 4.0f;
    params.depth = 6.0f;
    params.lockToBounds = false;

    uint32_t objectId = 0u;
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(layout, &params, &objectId, NULL));
    const Object3D* object = Layout_ObjectStore_FindConst(&layout->objectStore, objectId);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(object->kind == OBJECT3D_KIND_RECT_PRISM);

    Vec3 corners[8] = {0};
    TEST_ASSERT(Layout_Object3D_ComputeRectPrismCorners(object, corners));
    TEST_ASSERT(ld_test_nearly_equal(Vec3_Distance(corners[0], corners[1]), 2.0f));
    TEST_ASSERT(ld_test_nearly_equal(Vec3_Distance(corners[1], corners[2]), 4.0f));
    TEST_ASSERT(ld_test_nearly_equal(Vec3_Distance(corners[0], corners[4]), 6.0f));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_json_v8_persists_plane_primitives_deterministically(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    PlanePrimitiveCreateParams params;
    Layout_PlanePrimitiveCreateParams_SetDefaults(&params);
    params.width = 2.5f;
    params.height = 3.5f;
    params.lockToBounds = false;

    uint32_t id1 = 0u;
    uint32_t id2 = 0u;
    bool adjusted = false;
    layout->scene3d.constructionPlane.axisAligned = (ViewPlane){ .axis = VIEW_PLANE_XY, .offset = 1.25f };
    TEST_ASSERT(Layout_CreatePlanePrimitive(layout, &params, &id1, &adjusted));
    TEST_ASSERT(id1 == 1u);

    layout->scene3d.constructionPlane.axisAligned = (ViewPlane){ .axis = VIEW_PLANE_YZ, .offset = -0.75f };
    TEST_ASSERT(Layout_CreatePlanePrimitive(layout, &params, &id2, &adjusted));
    TEST_ASSERT(id2 == 2u);

    char* first = Layout_SaveToString(layout);
    TEST_ASSERT(first != NULL);
    TEST_ASSERT(Layout_LoadFromString(layout, first));
    char* second = Layout_SaveToString(layout);
    TEST_ASSERT(second != NULL);
    TEST_ASSERT(strcmp(first, second) == 0);

    const Object3D* loaded1 = Layout_ObjectStore_FindConst(&layout->objectStore, id1);
    const Object3D* loaded2 = Layout_ObjectStore_FindConst(&layout->objectStore, id2);
    TEST_ASSERT(loaded1 != NULL);
    TEST_ASSERT(loaded2 != NULL);
    TEST_ASSERT(loaded1->kind == OBJECT3D_KIND_PLANE);
    TEST_ASSERT(loaded2->kind == OBJECT3D_KIND_PLANE);
    TEST_ASSERT(ld_test_nearly_equal(loaded1->plane.width, 2.5f));
    TEST_ASSERT(ld_test_nearly_equal(loaded2->plane.height, 3.5f));

    {
        uint32_t id3 = 0u;
        layout->scene3d.constructionPlane.axisAligned = (ViewPlane){ .axis = VIEW_PLANE_XZ, .offset = 0.5f };
        TEST_ASSERT(Layout_CreatePlanePrimitive(layout, &params, &id3, &adjusted));
        TEST_ASSERT(id3 == 3u);
    }

    free(first);
    free(second);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_object_store_rect_prism_payload_validation(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    LayoutObjectStore* store = &state->layout.objectStore;

    uint32_t id = Layout_ObjectStore_Create(store,
                                            OBJECT3D_KIND_RECT_PRISM,
                                            NULL,
                                            "rect_prism_test",
                                            CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D,
                                            CORE_OBJECT_PLANE_XY);
    TEST_ASSERT(id > 0u);

    Object3D* object = Layout_ObjectStore_Find(store, id);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(object->kind == OBJECT3D_KIND_RECT_PRISM);
    TEST_ASSERT(Layout_ObjectStore_ValidateObject(object));

    object->rectPrism.width = 0.0f;
    TEST_ASSERT(!Layout_ObjectStore_ValidateObject(object));
    object->rectPrism.width = 2.0f;
    TEST_ASSERT(Layout_ObjectStore_ValidateObject(object));

    object->rectPrism.depth = 0.0f;
    TEST_ASSERT(Layout_ObjectStore_ValidateObject(object));
    object->rectPrism.depth = -1.0f;
    TEST_ASSERT(!Layout_ObjectStore_ValidateObject(object));
    object->rectPrism.depth = 3.0f;
    TEST_ASSERT(Layout_ObjectStore_ValidateObject(object));

    object->rectPrism.frame.axisV = object->rectPrism.frame.axisU;
    TEST_ASSERT(!Layout_ObjectStore_ValidateObject(object));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_json_v8_persists_rect_prism_payload_deterministically(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    Transform3D t1 = Layout_Transform3D_Default();
    t1.position = (Vec3){ 1.0f, 2.0f, 3.0f };
    uint32_t id1 = Layout_ObjectStore_Create(&layout->objectStore,
                                             OBJECT3D_KIND_RECT_PRISM,
                                             &t1,
                                             "rect_prism_primitive",
                                             CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D,
                                             CORE_OBJECT_PLANE_XY);
    TEST_ASSERT(id1 == 1u);
    Object3D* p1 = Layout_ObjectStore_Find(&layout->objectStore, id1);
    TEST_ASSERT(p1 != NULL);
    p1->rectPrism.width = 2.0f;
    p1->rectPrism.height = 3.0f;
    p1->rectPrism.depth = 4.0f;
    p1->rectPrism.lockToConstructionPlane = false;
    p1->rectPrism.lockToBounds = true;
    p1->rectPrism.frame.origin = p1->transform.position;
    TEST_ASSERT(Layout_ObjectStore_ValidateObject(p1));
    {
        Object3D baseline = *p1;
        TEST_ASSERT(Layout_RotateObject3D(layout,
                                          id1,
                                          (Vec3){ 0.0f, 1.0f, 0.0f },
                                          32.5f,
                                          &baseline,
                                          NULL));
    }
    TEST_ASSERT(fabsf(p1->transform.rotationDeg.y) > 0.1f);

    Transform3D t2 = Layout_Transform3D_Default();
    t2.position = (Vec3){ -3.0f, 0.5f, 1.5f };
    uint32_t id2 = Layout_ObjectStore_Create(&layout->objectStore,
                                             OBJECT3D_KIND_RECT_PRISM,
                                             &t2,
                                             "rect_prism_primitive",
                                             CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D,
                                             CORE_OBJECT_PLANE_XY);
    TEST_ASSERT(id2 == 2u);
    Object3D* p2 = Layout_ObjectStore_Find(&layout->objectStore, id2);
    TEST_ASSERT(p2 != NULL);
    p2->rectPrism.width = 1.5f;
    p2->rectPrism.height = 2.5f;
    p2->rectPrism.depth = 0.75f;
    p2->rectPrism.lockToConstructionPlane = true;
    p2->rectPrism.lockToBounds = false;
    p2->rectPrism.frame.origin = p2->transform.position;
    TEST_ASSERT(Layout_ObjectStore_ValidateObject(p2));

    char* first = Layout_SaveToString(layout);
    TEST_ASSERT(first != NULL);

    cJSON* root = cJSON_Parse(first);
    TEST_ASSERT(root != NULL);
    {
        const cJSON* file = cJSON_GetObjectItem(root, "file");
        const cJSON* version = cJSON_IsObject(file) ? cJSON_GetObjectItem(file, "schemaVersion") : NULL;
        const cJSON* objects3d = cJSON_GetObjectItem(root, "objects3d");
        TEST_ASSERT(cJSON_IsNumber(version));
        TEST_ASSERT(version->valueint == LAYOUT_JSON_SCHEMA_VERSION);
        TEST_ASSERT(cJSON_IsArray(objects3d));
        TEST_ASSERT(cJSON_GetArraySize(objects3d) == 2);
        const cJSON* obj0 = cJSON_GetArrayItem(objects3d, 0);
        const cJSON* rp0 = cJSON_IsObject(obj0) ? cJSON_GetObjectItem(obj0, "rectPrism") : NULL;
        TEST_ASSERT(cJSON_IsObject(rp0));
        TEST_ASSERT(ld_test_nearly_equal((float)cJSON_GetObjectItem(rp0, "width")->valuedouble, 2.0f));
        TEST_ASSERT(ld_test_nearly_equal((float)cJSON_GetObjectItem(rp0, "depth")->valuedouble, 4.0f));
    }
    cJSON_Delete(root);

    TEST_ASSERT(Layout_LoadFromString(layout, first));
    char* second = Layout_SaveToString(layout);
    TEST_ASSERT(second != NULL);
    TEST_ASSERT(strcmp(first, second) == 0);

    const Object3D* loaded1 = Layout_ObjectStore_FindConst(&layout->objectStore, id1);
    const Object3D* loaded2 = Layout_ObjectStore_FindConst(&layout->objectStore, id2);
    TEST_ASSERT(loaded1 != NULL);
    TEST_ASSERT(loaded2 != NULL);
    TEST_ASSERT(loaded1->kind == OBJECT3D_KIND_RECT_PRISM);
    TEST_ASSERT(loaded2->kind == OBJECT3D_KIND_RECT_PRISM);
    TEST_ASSERT(ld_test_nearly_equal(loaded1->rectPrism.width, 2.0f));
    TEST_ASSERT(ld_test_nearly_equal(loaded1->rectPrism.height, 3.0f));
    TEST_ASSERT(ld_test_nearly_equal(loaded1->rectPrism.depth, 4.0f));
    TEST_ASSERT(loaded1->rectPrism.lockToBounds);
    TEST_ASSERT(ld_test_nearly_equal(loaded1->transform.rotationDeg.y, 32.5f));
    TEST_ASSERT(ld_test_nearly_equal(loaded2->rectPrism.width, 1.5f));
    TEST_ASSERT(ld_test_nearly_equal(loaded2->rectPrism.height, 2.5f));
    TEST_ASSERT(ld_test_nearly_equal(loaded2->rectPrism.depth, 0.75f));
    TEST_ASSERT(loaded2->rectPrism.lockToConstructionPlane);

    free(first);
    free(second);
    ld_test_shutdown_runtime();
    return true;
}

bool test_layout_object3d_store_run_tests(void) {
    const TestCase cases[] = {
        { "ObjectStoreIdStabilityAndTombstoneDelete", test_object_store_id_stability_and_tombstone_delete },
        { "ObjectStorePlaneLockDimensionalRulesEnforced", test_object_store_plane_lock_dimensional_rules_enforced },
        { "ObjectStoreRejectsInvalidTransformScale", test_object_store_rejects_invalid_transform_scale },
        { "PlanePrimitiveCreationRespectsBoundsAndConstructionPlane", test_plane_primitive_creation_respects_bounds_and_construction_plane },
        { "RectPrismPrimitiveCreationRespectsBoundsAndConstructionPlane", test_rect_prism_primitive_creation_respects_bounds_and_construction_plane },
        { "RectPrismPrimitiveCreationClampsDepthToZeroAtBoundsLimit", test_rect_prism_primitive_creation_clamps_depth_to_zero_at_bounds_limit },
        { "LayoutObject3DComputeRectPrismCornersContract", test_layout_object3d_compute_rect_prism_corners_contract },
        { "LayoutJsonV8PersistsPlanePrimitivesDeterministically", test_layout_json_v8_persists_plane_primitives_deterministically },
        { "ObjectStoreRectPrismPayloadValidation", test_object_store_rect_prism_payload_validation },
        { "LayoutJsonV8PersistsRectPrismPayloadDeterministically", test_layout_json_v8_persists_rect_prism_payload_deterministically }
    };

    return run_test_cases("LayoutObject3DStore", cases, sizeof(cases) / sizeof(cases[0]));
}
