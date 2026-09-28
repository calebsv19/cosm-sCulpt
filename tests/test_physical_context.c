#include "test_layout_internal.h"

static uint32_t add_panel(Layout* layout) {
    PlanePrimitiveCreateParams p = {
        .width = 1.0f, .height = 0.5f, .useExplicitFrame = true,
        .explicitFrame = {.axisU = {1,0,0}, .axisV = {0,1,0}, .normal = {0,0,1}}
    };
    uint32_t id = 0;
    bool adjusted = false;
    return Layout_CreatePlanePrimitive(layout, &p, &id, &adjusted) ? id : 0;
}

static bool test_physical_context_and_identity_roundtrip(void) {
    Layout a, b;
    Layout_Init(&a, 1);
    Layout_Init(&b, 1);
    a.metersPerWorldUnit = 0.0254;
    uint32_t id = add_panel(&a);
    TEST_ASSERT(id != 0);
    Object3D* object = Layout_ObjectStore_Find(&a.objectStore, id);
    TEST_ASSERT(core_object_set_identity(&object->coreMeta, "panel.stable", "plane_primitive").code == CORE_OK);
    char* saved = Layout_SaveToString(&a);
    TEST_ASSERT(saved && Layout_LoadFromString(&b, saved));
    TEST_ASSERT(fabs(Layout_WorldScale(&b) - 0.0254) < 1e-12);
    TEST_ASSERT(strcmp(b.objectStore.items[0].coreMeta.object_id, "panel.stable") == 0);
    TEST_ASSERT(b.objectStore.items[0].objectId == id);
    TEST_ASSERT(b.objectStore.items[0].plane.width == 1.0f);
    Layout_FreeString(saved);
    Layout_Free(&a); Layout_Free(&b);
    return true;
}

static bool test_legacy_layout_default_and_invalid_context_atomicity(void) {
    Layout layout;
    Layout_Init(&layout, 1);
    TEST_ASSERT(Layout_LoadFromString(&layout, "{\"file\":{\"schemaVersion\":9,\"gridSize\":1},\"anchors\":[],\"walls\":[]}"));
    TEST_ASSERT(Layout_WorldScale(&layout) == 1.0);
    TEST_ASSERT(add_panel(&layout) != 0);
    char* before = Layout_SaveToString(&layout);
    const char* invalid[] = {
        "{\"file\":{\"schemaVersion\":10}}",
        "{\"file\":{\"schemaVersion\":10},\"physicalContext\":{\"coordinateSystem\":\"y_up\",\"metersPerWorldUnit\":1}}",
        "{\"file\":{\"schemaVersion\":10},\"physicalContext\":{\"coordinateSystem\":\"right_handed_z_up_meters\",\"metersPerWorldUnit\":-1}}"
    };
    for (size_t i = 0; i < sizeof(invalid)/sizeof(invalid[0]); ++i) {
        TEST_ASSERT(!Layout_LoadFromString(&layout, invalid[i]));
        char* after = Layout_SaveToString(&layout);
        TEST_ASSERT(after && strcmp(before, after) == 0);
        Layout_FreeString(after);
    }
    Layout_FreeString(before); Layout_Free(&layout);
    return true;
}

static bool test_duplicate_persistent_ids_reject_without_replacing_document(void) {
    Layout layout;
    Layout_Init(&layout, 1);
    TEST_ASSERT(add_panel(&layout) != 0);
    TEST_ASSERT(add_panel(&layout) != 0);
    char* before = Layout_SaveToString(&layout);
    cJSON* root = cJSON_Parse(before);
    cJSON* objects = cJSON_GetObjectItemCaseSensitive(root, "objects3d");
    cJSON* first = cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(objects, 0), "persistentId");
    cJSON* second = cJSON_GetArrayItem(objects, 1);
    TEST_ASSERT(cJSON_ReplaceItemInObjectCaseSensitive(second, "persistentId", cJSON_CreateString(first->valuestring)));
    char* invalid = cJSON_PrintUnformatted(root);
    TEST_ASSERT(!Layout_LoadFromString(&layout, invalid));
    char* after = Layout_SaveToString(&layout);
    TEST_ASSERT(after && strcmp(before, after) == 0);
    free(invalid); free(after); free(before); cJSON_Delete(root); Layout_Free(&layout);
    return true;
}

static bool test_display_uses_retained_scale(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    state->layout.metersPerWorldUnit = 0.0254;
    UIPanel_Get()->displayUnit = CORE_UNIT_MILLIMETER;
    double value = 0;
    TEST_ASSERT(UIPanel_ConvertWorldToDisplay(3.5, &value));
    TEST_ASSERT(fabs(value - 88.9) < 1e-9);
    TEST_ASSERT(UIPanel_ConvertDisplayToWorld(88.9, &value));
    TEST_ASSERT(fabs(value - 3.5) < 1e-9);
    state->layout.metersPerWorldUnit = 1.0;
    return true;
}

bool physical_context_run_tests(void) {
    const TestCase cases[] = {
        {"physical_context_identity_roundtrip", test_physical_context_and_identity_roundtrip},
        {"legacy_and_invalid_context", test_legacy_layout_default_and_invalid_context_atomicity},
        {"duplicate_identity_rejection", test_duplicate_persistent_ids_reject_without_replacing_document},
        {"display_uses_retained_scale", test_display_uses_retained_scale}
    };
    return run_test_cases("PhysicalContext", cases, sizeof(cases)/sizeof(cases[0]));
}
