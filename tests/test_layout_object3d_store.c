#include "test_layout_internal.h"
#include "Layout/asset/layout_object_asset_mesh_authoring.h"

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

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

    Layout_FreeString(first);
    Layout_FreeString(second);
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

    Layout_FreeString(first);
    Layout_FreeString(second);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_json_v9_persists_mesh_asset_instance_payload(void) {
    const char* runtime_path = "/tmp/ld_mesh_asset_instance_store.runtime.json";
    const char* runtime_json =
        "{"
        "\"schema_variant\":\"mesh_asset_runtime_v1\","
        "\"asset_id\":\"asset_test_mesh\","
        "\"source_asset_id\":\"source_test_mesh\","
        "\"vertex_count\":4,"
        "\"triangle_count\":2,"
        "\"local_bounds\":{"
            "\"min\":{\"x\":-1.0,\"y\":-2.0,\"z\":-3.0},"
            "\"max\":{\"x\":1.0,\"y\":2.0,\"z\":3.0}"
        "}"
        "}";
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;
    Transform3D transform = Layout_Transform3D_Default();
    uint32_t object_id = 0u;
    char diagnostics[256] = {0};

    transform.position = (Vec3){ 2.0f, 3.0f, 4.0f };
    transform.scale = (Vec3){ 1.5f, 2.0f, 0.5f };
    TEST_ASSERT(ld_test_write_text_file_basic(runtime_path, runtime_json));
    TEST_ASSERT(Layout_CreateMeshAssetInstanceFromRuntimeAsset(layout,
                                                              runtime_path,
                                                              &transform,
                                                              &object_id,
                                                              diagnostics,
                                                              sizeof(diagnostics)));
    TEST_ASSERT(object_id == 1u);

    const Object3D* object = Layout_ObjectStore_FindConst(&layout->objectStore, object_id);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(object->kind == OBJECT3D_KIND_MESH_ASSET_INSTANCE);
    TEST_ASSERT(strcmp(object->meshInstance.assetId, "asset_test_mesh") == 0);
    TEST_ASSERT(strcmp(object->meshInstance.sourceAssetId, "source_test_mesh") == 0);
    TEST_ASSERT(strcmp(object->meshInstance.runtimePath, runtime_path) == 0);
    TEST_ASSERT(object->meshInstance.vertexCount == 4u);
    TEST_ASSERT(object->meshInstance.triangleCount == 2u);
    TEST_ASSERT(ld_test_vec3_nearly_equal(object->meshInstance.localBoundsMin,
                                          (Vec3){ -1.0f, -2.0f, -3.0f }));
    TEST_ASSERT(ld_test_vec3_nearly_equal(object->meshInstance.localBoundsMax,
                                          (Vec3){ 1.0f, 2.0f, 3.0f }));

    char* first = Layout_SaveToString(layout);
    TEST_ASSERT(first != NULL);
    {
        cJSON* root = cJSON_Parse(first);
        TEST_ASSERT(root != NULL);
        const cJSON* file = cJSON_GetObjectItem(root, "file");
        const cJSON* version = cJSON_IsObject(file) ? cJSON_GetObjectItem(file, "schemaVersion") : NULL;
        const cJSON* objects3d = cJSON_GetObjectItem(root, "objects3d");
        const cJSON* obj0 = cJSON_GetArrayItem(objects3d, 0);
        const cJSON* mesh = cJSON_IsObject(obj0) ? cJSON_GetObjectItem(obj0, "meshAssetInstance") : NULL;
        TEST_ASSERT(cJSON_IsNumber(version));
        TEST_ASSERT(version->valueint == LAYOUT_JSON_SCHEMA_VERSION_OBJECT3D_MESH_INSTANCE);
        TEST_ASSERT(cJSON_IsObject(mesh));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(mesh, "assetId")->valuestring, "asset_test_mesh") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(mesh, "runtimePath")->valuestring, runtime_path) == 0);
        TEST_ASSERT(cJSON_GetObjectItem(mesh, "triangleCount")->valueint == 2);
        cJSON_Delete(root);
    }

    TEST_ASSERT(Layout_LoadFromString(layout, first));
    char* second = Layout_SaveToString(layout);
    TEST_ASSERT(second != NULL);
    TEST_ASSERT(strcmp(first, second) == 0);
    object = Layout_ObjectStore_FindConst(&layout->objectStore, object_id);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(object->kind == OBJECT3D_KIND_MESH_ASSET_INSTANCE);
    TEST_ASSERT(strcmp(object->meshInstance.assetId, "asset_test_mesh") == 0);
    TEST_ASSERT(ld_test_nearly_equal(object->transform.scale.y, 2.0f));

    Layout_FreeString(first);
    Layout_FreeString(second);
    remove(runtime_path);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_workspace_mode_handoff_seeds_object_workspace_selection_and_focus(void) {
    GlobalState* state = NULL;
    Layout* layout = NULL;
    PlanePrimitiveCreateParams params;
    uint32_t scene_object_id = 0u;
    bool adjusted = false;

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);
    layout = &state->layout;

    params = (PlanePrimitiveCreateParams){
        .width = 6.0f,
        .height = 4.0f,
        .useExplicitFrame = true,
        .explicitFrame = {
            .origin = { 14.0f, -8.0f, 3.0f },
            .axisU = { 1.0f, 0.0f, 0.0f },
            .axisV = { 0.0f, 1.0f, 0.0f },
            .normal = { 0.0f, 0.0f, 1.0f }
        },
        .lockToConstructionPlane = true,
        .lockToBounds = false
    };
    TEST_ASSERT(Layout_CreatePlanePrimitive(layout, &params, &scene_object_id, &adjusted));
    state->editor.selectedObject3DId = scene_object_id;
    state->grid.scale = 5.0f;
    state->grid.offsetX = 27.0f;
    state->grid.offsetY = -11.0f;
    state->activePlane = (ViewPlane){ .axis = VIEW_PLANE_XZ, .offset = 2.0f };
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.target = (Vec3){ 50.0f, 60.0f, 70.0f };

    TEST_ASSERT(Global_SetWorkspaceMode(LINE_DRAWING_WORKSPACE_MODE_OBJECT));
    TEST_ASSERT(state->workspaceMode == LINE_DRAWING_WORKSPACE_MODE_OBJECT);
    TEST_ASSERT(Layout_ObjectStore_LiveCount(&state->layout.objectStore) == 1u);
    TEST_ASSERT(state->editor.selectedObject3DId == 1u);
    TEST_ASSERT(state->editor.selectedObjectAssetBodyId == 1u);
    TEST_ASSERT(state->editor.selectedObjectAssetFace == OBJECT3D_FACE_PLANE_SURFACE);
    TEST_ASSERT(state->layout.scene3d.bounds.enabled);
    TEST_ASSERT(state->freeViewCamera.enabled);
    TEST_ASSERT(ld_test_vec3_nearly_equal(state->freeViewCamera.target, (Vec3){ 0.0f, 0.0f, 0.0f }));
    TEST_ASSERT(state->grid.scale > 30.0f);
    TEST_ASSERT(state->sceneWorkspaceDocument.hasViewportState);
    TEST_ASSERT(ld_test_nearly_equal(state->sceneWorkspaceDocument.grid.scale, 5.0f));
    TEST_ASSERT(ld_test_vec3_nearly_equal(state->sceneWorkspaceDocument.freeViewCamera.target,
                                          (Vec3){ 50.0f, 60.0f, 70.0f }));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_workspace_mode_handoff_restores_scene_and_object_viewports(void) {
    GlobalState* state = NULL;
    Layout* layout = NULL;
    RectPrismPrimitiveCreateParams params;
    uint32_t scene_object_id = 0u;
    bool adjusted = false;

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);
    layout = &state->layout;

    params = (RectPrismPrimitiveCreateParams){
        .width = 4.0f,
        .height = 5.0f,
        .depth = 6.0f,
        .useExplicitFrame = true,
        .explicitFrame = {
            .origin = { -9.0f, 12.0f, 1.5f },
            .axisU = { 1.0f, 0.0f, 0.0f },
            .axisV = { 0.0f, 1.0f, 0.0f },
            .normal = { 0.0f, 0.0f, 1.0f }
        },
        .lockToConstructionPlane = false,
        .lockToBounds = false
    };
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(layout, &params, &scene_object_id, &adjusted));
    state->editor.selectedObject3DId = scene_object_id;
    state->grid.scale = 7.0f;
    state->grid.offsetX = 13.0f;
    state->grid.offsetY = -4.5f;
    state->activePlane = (ViewPlane){ .axis = VIEW_PLANE_YZ, .offset = -2.0f };
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.yawDeg = 61.0f;
    state->freeViewCamera.pitchDeg = 14.0f;
    state->freeViewCamera.target = (Vec3){ 3.0f, 4.0f, 5.0f };

    TEST_ASSERT(Global_SetWorkspaceMode(LINE_DRAWING_WORKSPACE_MODE_OBJECT));
    state->grid.scale = 19.0f;
    state->grid.offsetX = -8.0f;
    state->grid.offsetY = 2.5f;
    state->activePlane = (ViewPlane){ .axis = VIEW_PLANE_XY, .offset = 6.0f };
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.yawDeg = 88.0f;
    state->freeViewCamera.pitchDeg = 11.0f;
    state->freeViewCamera.target = (Vec3){ 0.5f, 1.5f, 2.5f };

    TEST_ASSERT(Global_SetWorkspaceMode(LINE_DRAWING_WORKSPACE_MODE_SCENE));
    TEST_ASSERT(state->workspaceMode == LINE_DRAWING_WORKSPACE_MODE_SCENE);
    TEST_ASSERT(state->editor.selectedObject3DId == scene_object_id);
    TEST_ASSERT(ld_test_nearly_equal(state->grid.scale, 7.0f));
    TEST_ASSERT(ld_test_nearly_equal(state->grid.offsetX, 13.0f));
    TEST_ASSERT(ld_test_nearly_equal(state->grid.offsetY, -4.5f));
    TEST_ASSERT(state->activePlane.axis == VIEW_PLANE_YZ);
    TEST_ASSERT(ld_test_nearly_equal(state->activePlane.offset, -2.0f));
    TEST_ASSERT(ld_test_nearly_equal(state->freeViewCamera.yawDeg, 61.0f));
    TEST_ASSERT(ld_test_nearly_equal(state->freeViewCamera.pitchDeg, 14.0f));
    TEST_ASSERT(ld_test_vec3_nearly_equal(state->freeViewCamera.target, (Vec3){ 3.0f, 4.0f, 5.0f }));
    TEST_ASSERT(state->objectWorkspaceDocument.hasViewportState);
    TEST_ASSERT(ld_test_nearly_equal(state->objectWorkspaceDocument.grid.scale, 19.0f));
    TEST_ASSERT(ld_test_nearly_equal(state->objectWorkspaceDocument.grid.offsetX, -8.0f));
    TEST_ASSERT(ld_test_nearly_equal(state->objectWorkspaceDocument.grid.offsetY, 2.5f));

    TEST_ASSERT(Global_SetWorkspaceMode(LINE_DRAWING_WORKSPACE_MODE_OBJECT));
    TEST_ASSERT(state->workspaceMode == LINE_DRAWING_WORKSPACE_MODE_OBJECT);
    TEST_ASSERT(state->editor.selectedObjectAssetBodyId == 1u);
    TEST_ASSERT(state->editor.selectedObjectAssetFace != OBJECT3D_FACE_NONE);
    TEST_ASSERT(ld_test_nearly_equal(state->grid.scale, 19.0f));
    TEST_ASSERT(ld_test_nearly_equal(state->grid.offsetX, -8.0f));
    TEST_ASSERT(ld_test_nearly_equal(state->grid.offsetY, 2.5f));
    TEST_ASSERT(state->activePlane.axis == VIEW_PLANE_XY);
    TEST_ASSERT(ld_test_nearly_equal(state->activePlane.offset, 6.0f));
    TEST_ASSERT(ld_test_nearly_equal(state->freeViewCamera.yawDeg, 88.0f));
    TEST_ASSERT(ld_test_nearly_equal(state->freeViewCamera.pitchDeg, 11.0f));
    TEST_ASSERT(ld_test_vec3_nearly_equal(state->freeViewCamera.target, (Vec3){ 0.5f, 1.5f, 2.5f }));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_workspace_mode_handoff_reseeds_object_workspace_when_scene_selection_changes(void) {
    GlobalState* state = NULL;
    Layout* layout = NULL;
    PlanePrimitiveCreateParams plane_params;
    RectPrismPrimitiveCreateParams prism_params;
    uint32_t plane_scene_object_id = 0u;
    uint32_t prism_scene_object_id = 0u;
    bool adjusted = false;
    const Object3D* selected_object = NULL;

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);
    layout = &state->layout;

    plane_params = (PlanePrimitiveCreateParams){
        .width = 6.0f,
        .height = 4.0f,
        .useExplicitFrame = true,
        .explicitFrame = {
            .origin = { 0.0f, 0.0f, 0.0f },
            .axisU = { 1.0f, 0.0f, 0.0f },
            .axisV = { 0.0f, 1.0f, 0.0f },
            .normal = { 0.0f, 0.0f, 1.0f }
        },
        .lockToConstructionPlane = true,
        .lockToBounds = false
    };
    TEST_ASSERT(Layout_CreatePlanePrimitive(layout, &plane_params, &plane_scene_object_id, &adjusted));

    prism_params = (RectPrismPrimitiveCreateParams){
        .width = 4.0f,
        .height = 5.0f,
        .depth = 6.0f,
        .useExplicitFrame = true,
        .explicitFrame = {
            .origin = { 12.0f, -9.0f, 3.0f },
            .axisU = { 1.0f, 0.0f, 0.0f },
            .axisV = { 0.0f, 1.0f, 0.0f },
            .normal = { 0.0f, 0.0f, 1.0f }
        },
        .lockToConstructionPlane = false,
        .lockToBounds = false
    };
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(layout, &prism_params, &prism_scene_object_id, &adjusted));

    state->editor.selectedObject3DId = plane_scene_object_id;
    TEST_ASSERT(Global_SetWorkspaceMode(LINE_DRAWING_WORKSPACE_MODE_OBJECT));
    TEST_ASSERT(state->objectWorkspaceDocument.workspaceSourceSceneObjectId == plane_scene_object_id);
    TEST_ASSERT(Global_SetWorkspaceMode(LINE_DRAWING_WORKSPACE_MODE_SCENE));

    state->editor.selectedObject3DId = prism_scene_object_id;
    TEST_ASSERT(Global_SetWorkspaceMode(LINE_DRAWING_WORKSPACE_MODE_OBJECT));
    TEST_ASSERT(state->workspaceMode == LINE_DRAWING_WORKSPACE_MODE_OBJECT);
    TEST_ASSERT(state->objectWorkspaceDocument.workspaceSourceSceneObjectId == prism_scene_object_id);
    TEST_ASSERT(Layout_ObjectStore_LiveCount(&state->layout.objectStore) == 1u);
    TEST_ASSERT(state->editor.selectedObject3DId == 1u);
    TEST_ASSERT(state->editor.selectedObjectAssetBodyId == 1u);
    selected_object =
        Layout_ObjectStore_FindConst(&state->layout.objectStore, state->editor.selectedObject3DId);
    TEST_ASSERT(selected_object != NULL);
    TEST_ASSERT(selected_object->kind == OBJECT3D_KIND_RECT_PRISM);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_workspace_mode_handoff_reopens_mesh_instance_source_asset(void) {
    GlobalState* state = NULL;
    Layout asset_layout;
    ObjectAuthoringSession asset_authoring;
    RectPrismPrimitiveCreateParams params;
    uint32_t asset_body_id = 0u;
    uint32_t scene_object_id = 0u;
    bool adjusted = false;
    char asset_root[LINE_DRAWING_PATH_CAP];
    char source_asset_path[LINE_DRAWING_PATH_CAP];
    char runtime_path[LINE_DRAWING_PATH_CAP];
    char runtime_json[768];
    char diagnostics[256] = {0};
    const Object3D* selected_object = NULL;

    snprintf(asset_root,
             sizeof(asset_root),
             "/tmp/ld_mesh_source_reopen_%u",
             (unsigned)SDL_GetTicks());
    TEST_ASSERT(mkdir(asset_root, 0755) == 0 || errno == EEXIST);
    snprintf(source_asset_path, sizeof(source_asset_path), "%s/reopen_asset.json", asset_root);
    snprintf(runtime_path, sizeof(runtime_path), "%s/reopen_asset.runtime.json", asset_root);

    Layout_Init(&asset_layout, 1.0f);
    ObjectAuthoringSession_Init(&asset_authoring);
    params = (RectPrismPrimitiveCreateParams){
        .width = 3.0f,
        .height = 4.0f,
        .depth = 5.0f,
        .useExplicitFrame = true,
        .explicitFrame = {
            .origin = { 0.0f, 0.0f, 0.0f },
            .axisU = { 1.0f, 0.0f, 0.0f },
            .axisV = { 0.0f, 1.0f, 0.0f },
            .normal = { 0.0f, 0.0f, 1.0f }
        },
        .lockToConstructionPlane = false,
        .lockToBounds = false
    };
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(&asset_layout, &params, &asset_body_id, &adjusted));
    TEST_ASSERT(ObjectAuthoringSession_ResetFromLayout(&asset_authoring, &asset_layout, 0u));
    TEST_ASSERT(LayoutObjectAssetMeshAuthoring_SaveWithAuthoring(&asset_layout,
                                                                 &asset_authoring.document,
                                                                 source_asset_path,
                                                                 diagnostics,
                                                                 sizeof(diagnostics)));
    ObjectAuthoringSession_Free(&asset_authoring);
    Layout_Free(&asset_layout);

    snprintf(runtime_json,
             sizeof(runtime_json),
             "{"
             "\"schema_variant\":\"mesh_asset_runtime_v1\","
             "\"asset_id\":\"reopen_asset_runtime\","
             "\"source_asset_id\":\"reopen_asset\","
             "\"vertex_count\":8,"
             "\"triangle_count\":12,"
             "\"local_bounds\":{"
             "\"min\":{\"x\":-1.5,\"y\":-2.0,\"z\":-2.5},"
             "\"max\":{\"x\":1.5,\"y\":2.0,\"z\":2.5}"
             "}"
             "}\n");
    TEST_ASSERT(ld_test_write_text_file_basic(runtime_path, runtime_json));

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);
    TEST_ASSERT(Global_SetObjectAssetRoot(asset_root, false));
    TEST_ASSERT(Layout_CreateMeshAssetInstanceFromRuntimeAsset(&state->layout,
                                                              runtime_path,
                                                              NULL,
                                                              &scene_object_id,
                                                              diagnostics,
                                                              sizeof(diagnostics)));
    state->editor.selectedObject3DId = scene_object_id;

    TEST_ASSERT(Global_SetWorkspaceMode(LINE_DRAWING_WORKSPACE_MODE_OBJECT));
    TEST_ASSERT(state->workspaceMode == LINE_DRAWING_WORKSPACE_MODE_OBJECT);
    TEST_ASSERT(strcmp(Global_GetCurrentObjectAssetPath(), source_asset_path) == 0);
    TEST_ASSERT(state->objectWorkspaceDocument.workspaceSourceSceneObjectId == scene_object_id);
    TEST_ASSERT(Layout_ObjectStore_LiveCount(&state->layout.objectStore) == 1u);
    TEST_ASSERT(state->editor.selectedObject3DId != 0u);
    selected_object =
        Layout_ObjectStore_FindConst(&state->layout.objectStore, state->editor.selectedObject3DId);
    TEST_ASSERT(selected_object != NULL);
    TEST_ASSERT(selected_object->kind == OBJECT3D_KIND_RECT_PRISM);
    TEST_ASSERT(state->objectAuthoring.attached);
    TEST_ASSERT(state->objectAuthoring.sourceSceneObjectId == scene_object_id);
    TEST_ASSERT(state->objectAuthoring.document.bodyCount == 1u);
    TEST_ASSERT(state->editor.selectedObjectAssetBodyId == selected_object->objectId);
    TEST_ASSERT(state->editor.selectedObjectAssetFace != OBJECT3D_FACE_NONE);

    ld_test_shutdown_runtime();
    (void)unlink(source_asset_path);
    (void)unlink(runtime_path);
    (void)rmdir(asset_root);
    return true;
}

static bool test_workspace_mode_handoff_refreshes_mesh_instance_runtime_sidecar(void) {
    GlobalState* state = NULL;
    Layout asset_layout;
    ObjectAuthoringSession asset_authoring;
    RectPrismPrimitiveCreateParams params;
    Transform3D scene_transform;
    Transform3D second_scene_transform;
    uint32_t asset_body_id = 0u;
    uint32_t scene_object_id = 0u;
    uint32_t second_scene_object_id = 0u;
    bool adjusted = false;
    char asset_root[LINE_DRAWING_PATH_CAP];
    char source_asset_path[LINE_DRAWING_PATH_CAP];
    char runtime_path[LINE_DRAWING_PATH_CAP];
    char runtime_json[768];
    char diagnostics[256] = {0};
    const Object3D* scene_object = NULL;
    const Object3D* second_scene_object = NULL;

    snprintf(asset_root,
             sizeof(asset_root),
             "/tmp/ld_mesh_sidecar_refresh_%u",
             (unsigned)SDL_GetTicks());
    TEST_ASSERT(mkdir(asset_root, 0755) == 0 || errno == EEXIST);
    snprintf(source_asset_path, sizeof(source_asset_path), "%s/refresh_asset.json", asset_root);
    snprintf(runtime_path, sizeof(runtime_path), "%s/refresh_asset.runtime.json", asset_root);

    Layout_Init(&asset_layout, 1.0f);
    ObjectAuthoringSession_Init(&asset_authoring);
    params = (RectPrismPrimitiveCreateParams){
        .width = 2.0f,
        .height = 3.0f,
        .depth = 4.0f,
        .useExplicitFrame = true,
        .explicitFrame = {
            .origin = { 0.0f, 0.0f, 0.0f },
            .axisU = { 1.0f, 0.0f, 0.0f },
            .axisV = { 0.0f, 1.0f, 0.0f },
            .normal = { 0.0f, 0.0f, 1.0f }
        },
        .lockToConstructionPlane = false,
        .lockToBounds = false
    };
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(&asset_layout, &params, &asset_body_id, &adjusted));
    TEST_ASSERT(ObjectAuthoringSession_ResetFromLayout(&asset_authoring, &asset_layout, 0u));
    TEST_ASSERT(LayoutObjectAssetMeshAuthoring_SaveWithAuthoring(&asset_layout,
                                                                 &asset_authoring.document,
                                                                 source_asset_path,
                                                                 diagnostics,
                                                                 sizeof(diagnostics)));
    ObjectAuthoringSession_Free(&asset_authoring);
    Layout_Free(&asset_layout);

    snprintf(runtime_json,
             sizeof(runtime_json),
             "{"
             "\"schema_variant\":\"mesh_asset_runtime_v1\","
             "\"asset_id\":\"refresh_asset_runtime_v1\","
             "\"source_asset_id\":\"refresh_asset\","
             "\"vertex_count\":8,"
             "\"triangle_count\":12,"
             "\"local_bounds\":{"
             "\"min\":{\"x\":-1.0,\"y\":-1.5,\"z\":-2.0},"
             "\"max\":{\"x\":1.0,\"y\":1.5,\"z\":2.0}"
             "}"
             "}\n");
    TEST_ASSERT(ld_test_write_text_file_basic(runtime_path, runtime_json));

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);
    TEST_ASSERT(Global_SetObjectAssetRoot(asset_root, false));
    scene_transform = Layout_Transform3D_Default();
    scene_transform.position = (Vec3){ 7.0f, -3.0f, 2.5f };
    scene_transform.rotationDeg = (Vec3){ 0.0f, 45.0f, 0.0f };
    scene_transform.scale = (Vec3){ 1.25f, 1.25f, 1.25f };
    TEST_ASSERT(Layout_CreateMeshAssetInstanceFromRuntimeAsset(&state->layout,
                                                              runtime_path,
                                                              &scene_transform,
                                                              &scene_object_id,
                                                              diagnostics,
                                                              sizeof(diagnostics)));
    second_scene_transform = Layout_Transform3D_Default();
    second_scene_transform.position = (Vec3){ -5.0f, 6.0f, -1.5f };
    second_scene_transform.rotationDeg = (Vec3){ 15.0f, 0.0f, 30.0f };
    second_scene_transform.scale = (Vec3){ 0.75f, 1.5f, 1.0f };
    TEST_ASSERT(Layout_CreateMeshAssetInstanceFromRuntimeAsset(&state->layout,
                                                              runtime_path,
                                                              &second_scene_transform,
                                                              &second_scene_object_id,
                                                              diagnostics,
                                                              sizeof(diagnostics)));
    state->editor.selectedObject3DId = scene_object_id;
    state->layoutDirtySinceSave = false;

    TEST_ASSERT(Global_SetWorkspaceMode(LINE_DRAWING_WORKSPACE_MODE_OBJECT));
    TEST_ASSERT(state->workspaceMode == LINE_DRAWING_WORKSPACE_MODE_OBJECT);
    TEST_ASSERT(strcmp(Global_GetCurrentObjectAssetPath(), source_asset_path) == 0);

    snprintf(runtime_json,
             sizeof(runtime_json),
             "{"
             "\"schema_variant\":\"mesh_asset_runtime_v1\","
             "\"asset_id\":\"refresh_asset_runtime_v2\","
             "\"source_asset_id\":\"refresh_asset\","
             "\"vertex_count\":24,"
             "\"triangle_count\":36,"
             "\"local_bounds\":{"
             "\"min\":{\"x\":-2.0,\"y\":-3.0,\"z\":-4.0},"
             "\"max\":{\"x\":2.0,\"y\":3.0,\"z\":4.0}"
             "}"
             "}\n");
    TEST_ASSERT(ld_test_write_text_file_basic(runtime_path, runtime_json));

    TEST_ASSERT(Global_SetWorkspaceMode(LINE_DRAWING_WORKSPACE_MODE_SCENE));
    TEST_ASSERT(state->workspaceMode == LINE_DRAWING_WORKSPACE_MODE_SCENE);
    TEST_ASSERT(state->editor.selectedObject3DId == scene_object_id);
    TEST_ASSERT(Layout_ObjectStore_LiveCount(&state->layout.objectStore) == 2u);
    scene_object = Layout_ObjectStore_FindConst(&state->layout.objectStore, scene_object_id);
    second_scene_object =
        Layout_ObjectStore_FindConst(&state->layout.objectStore, second_scene_object_id);
    TEST_ASSERT(scene_object != NULL);
    TEST_ASSERT(second_scene_object != NULL);
    TEST_ASSERT(scene_object->kind == OBJECT3D_KIND_MESH_ASSET_INSTANCE);
    TEST_ASSERT(second_scene_object->kind == OBJECT3D_KIND_MESH_ASSET_INSTANCE);
    TEST_ASSERT(strcmp(scene_object->meshInstance.assetId, "refresh_asset_runtime_v2") == 0);
    TEST_ASSERT(strcmp(second_scene_object->meshInstance.assetId, "refresh_asset_runtime_v2") == 0);
    TEST_ASSERT(strcmp(scene_object->meshInstance.sourceAssetId, "refresh_asset") == 0);
    TEST_ASSERT(strcmp(second_scene_object->meshInstance.sourceAssetId, "refresh_asset") == 0);
    TEST_ASSERT(strcmp(scene_object->meshInstance.runtimePath, runtime_path) == 0);
    TEST_ASSERT(strcmp(second_scene_object->meshInstance.runtimePath, runtime_path) == 0);
    TEST_ASSERT(scene_object->meshInstance.vertexCount == 24u);
    TEST_ASSERT(second_scene_object->meshInstance.vertexCount == 24u);
    TEST_ASSERT(scene_object->meshInstance.triangleCount == 36u);
    TEST_ASSERT(second_scene_object->meshInstance.triangleCount == 36u);
    TEST_ASSERT(ld_test_vec3_nearly_equal(scene_object->meshInstance.localBoundsMin,
                                          (Vec3){ -2.0f, -3.0f, -4.0f }));
    TEST_ASSERT(ld_test_vec3_nearly_equal(second_scene_object->meshInstance.localBoundsMin,
                                          (Vec3){ -2.0f, -3.0f, -4.0f }));
    TEST_ASSERT(ld_test_vec3_nearly_equal(scene_object->meshInstance.localBoundsMax,
                                          (Vec3){ 2.0f, 3.0f, 4.0f }));
    TEST_ASSERT(ld_test_vec3_nearly_equal(second_scene_object->meshInstance.localBoundsMax,
                                          (Vec3){ 2.0f, 3.0f, 4.0f }));
    TEST_ASSERT(ld_test_vec3_nearly_equal(scene_object->transform.position,
                                          scene_transform.position));
    TEST_ASSERT(ld_test_vec3_nearly_equal(second_scene_object->transform.position,
                                          second_scene_transform.position));
    TEST_ASSERT(ld_test_vec3_nearly_equal(scene_object->transform.rotationDeg,
                                          scene_transform.rotationDeg));
    TEST_ASSERT(ld_test_vec3_nearly_equal(second_scene_object->transform.rotationDeg,
                                          second_scene_transform.rotationDeg));
    TEST_ASSERT(ld_test_vec3_nearly_equal(scene_object->transform.scale,
                                          scene_transform.scale));
    TEST_ASSERT(ld_test_vec3_nearly_equal(second_scene_object->transform.scale,
                                          second_scene_transform.scale));
    TEST_ASSERT(state->layoutDirtySinceSave);

    ld_test_shutdown_runtime();
    (void)unlink(source_asset_path);
    (void)unlink(runtime_path);
    (void)rmdir(asset_root);
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
        { "LayoutJsonV8PersistsRectPrismPayloadDeterministically", test_layout_json_v8_persists_rect_prism_payload_deterministically },
        { "LayoutJsonV9PersistsMeshAssetInstancePayload", test_layout_json_v9_persists_mesh_asset_instance_payload },
        { "WorkspaceModeHandoffSeedsObjectWorkspaceSelectionAndFocus", test_workspace_mode_handoff_seeds_object_workspace_selection_and_focus },
        { "WorkspaceModeHandoffRestoresSceneAndObjectViewports", test_workspace_mode_handoff_restores_scene_and_object_viewports },
        { "WorkspaceModeHandoffReseedsObjectWorkspaceWhenSceneSelectionChanges",
          test_workspace_mode_handoff_reseeds_object_workspace_when_scene_selection_changes },
        { "WorkspaceModeHandoffReopensMeshInstanceSourceAsset",
          test_workspace_mode_handoff_reopens_mesh_instance_source_asset },
        { "WorkspaceModeHandoffRefreshesMeshInstanceRuntimeSidecar",
          test_workspace_mode_handoff_refreshes_mesh_instance_runtime_sidecar }
    };

    return run_test_cases("LayoutObject3DStore", cases, sizeof(cases) / sizeof(cases[0]));
}
