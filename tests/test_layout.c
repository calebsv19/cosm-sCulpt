#include "test_framework.h"

#include "Core/global_state.h"
#include "Core/space_mode_adapter.h"
#include "Layout/layout.h"
#include "Layout/layout_json.h"
#include "Layout/hitbox_system.h"
#include "Editor/editor.h"
#include "Math/math_util.h"
#include "Tools/canonical_scene_export.h"
#include "Tools/shape_from_layout.h"
#include "UI/ui_panel.h"
#include "core_units.h"
#include "cjson/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static bool nearly_equal(float a, float b) {
    return fabsf(a - b) < 0.0001f;
}

static char* read_text_file(const char* path) {
    FILE* f = NULL;
    long len = 0;
    char* buf = NULL;

    if (!path) return NULL;
    f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    len = ftell(f);
    if (len < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    buf = (char*)malloc((size_t)len + 1u);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1u, (size_t)len, f) != (size_t)len) {
        fclose(f);
        free(buf);
        return NULL;
    }
    fclose(f);
    buf[len] = '\0';
    return buf;
}

static bool vec3_nearly_equal(Vec3 a, Vec3 b) {
    return nearly_equal(a.x, b.x) &&
           nearly_equal(a.y, b.y) &&
           nearly_equal(a.z, b.z);
}

static cJSON* find_object_by_id(cJSON* objects, const char* object_id) {
    int count = 0;
    int i = 0;
    if (!cJSON_IsArray(objects) || !object_id) return NULL;
    count = cJSON_GetArraySize(objects);
    for (i = 0; i < count; ++i) {
        cJSON* obj = cJSON_GetArrayItem(objects, i);
        cJSON* id = cJSON_GetObjectItem(obj, "object_id");
        if (cJSON_IsString(id) && id->valuestring && strcmp(id->valuestring, object_id) == 0) {
            return obj;
        }
    }
    return NULL;
}

static void init_runtime(void) {
    Global_Init(800, 600);
}

static void shutdown_runtime(void) {
    Global_Shutdown();
}

static bool test_layout_add_wall_reuses_anchors(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    Vec2 a = {0.0f, 0.0f};
    Vec2 b = {2.0f, 0.0f};

    Layout_AddWall(layout, a, b);
    TEST_ASSERT(layout->wallCount == 1);
    TEST_ASSERT(layout->anchorCount == 2);

    Layout_AddWall(layout, a, b);
    TEST_ASSERT(layout->wallCount == 2);
    TEST_ASSERT(layout->anchorCount == 2); // should reuse anchors

    shutdown_runtime();
    return true;
}

static bool test_layout_remove_wall_compacts_on_demand(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    Vec2 a = {0.0f, 0.0f};
    Vec2 b = {1.0f, 0.0f};

    Layout_AddWall(layout, a, b);
    Layout_RemoveWall(layout, 0);
    TEST_ASSERT(layout->walls[0].isDeleted);
    TEST_ASSERT(state->layoutDirty == true);

    Global_RebuildHitboxesIfDirty();

    TEST_ASSERT(layout->wallCount == 0);
    TEST_ASSERT(state->layoutDirty == false);
    TEST_ASSERT(state->hitboxDirty == false);

    shutdown_runtime();
    return true;
}

static bool test_layout_string_roundtrip(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    Vec2 a = {0.0f, 0.0f};
    Vec2 b = {1.0f, 1.0f};
    Layout_AddWall(layout, a, b);

    char* snapshot = Layout_SaveToString(layout);
    TEST_ASSERT(snapshot != NULL);

    TEST_ASSERT(Layout_LoadFromString(layout, snapshot));
    free(snapshot);

    TEST_ASSERT(layout->wallCount == 1);
    TEST_ASSERT(layout->anchorCount == 2);

    shutdown_runtime();
    return true;
}

static bool test_editor_undo_redo_restores_layout(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;
    EditorState* editor = &state->editor;

    Vec2 a = {0.0f, 0.0f};
    Vec2 b = {2.0f, 0.0f};

    Editor_HistoryCapture(editor, layout);
    Layout_AddWall(layout, a, b);
    TEST_ASSERT(layout->wallCount == 1);

    TEST_ASSERT(Editor_Undo(editor, layout));
    TEST_ASSERT(layout->wallCount == 0);

    TEST_ASSERT(Editor_Redo(editor, layout));
    TEST_ASSERT(layout->wallCount == 1);

    shutdown_runtime();
    return true;
}

static bool test_layout_json_embeds_version(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    Vec2 a = {0.0f, 0.0f};
    Vec2 b = {1.0f, 0.0f};
    Layout_AddWall(layout, a, b);

    char* json = Layout_SaveToString(layout);
    TEST_ASSERT(json != NULL);

    cJSON* root = cJSON_Parse(json);
    TEST_ASSERT(root != NULL);

    const cJSON* file = cJSON_GetObjectItem(root, "file");
    TEST_ASSERT(file && cJSON_IsObject(file));
    const cJSON* version = cJSON_GetObjectItem(file, "schemaVersion");
    TEST_ASSERT(version && cJSON_IsNumber(version));
    TEST_ASSERT(version->valueint == LAYOUT_JSON_SCHEMA_VERSION);

    cJSON_Delete(root);
    free(json);
    shutdown_runtime();
    return true;
}

static bool test_layout_json_future_version_rejected(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    Vec2 a = {0.0f, 0.0f};
    Vec2 b = {1.0f, 0.0f};
    Layout_AddWall(layout, a, b);

    char* baseline = Layout_SaveToString(layout);
    TEST_ASSERT(baseline != NULL);

    const char* futureJson = "{\"file\":{\"schemaVersion\":999,\"gridSize\":1},\"anchors\":[],\"walls\":[]}";
    TEST_ASSERT(!Layout_LoadFromString(layout, futureJson));

    char* after = Layout_SaveToString(layout);
    TEST_ASSERT(after != NULL);
    TEST_ASSERT(strcmp(baseline, after) == 0);

    free(baseline);
    free(after);
    shutdown_runtime();
    return true;
}

static bool test_layout_json_missing_version_defaults(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    const char* legacyJson = "{\"anchors\":[{\"x\":0,\"y\":0,\"persistent\":true}],\"walls\":[]}";
    TEST_ASSERT(Layout_LoadFromString(layout, legacyJson));
    TEST_ASSERT(layout->anchorCount == 1);
    TEST_ASSERT(layout->anchors[0].isPersistent == true);
    TEST_ASSERT(layout->gridSize == 1.0f);
    TEST_ASSERT(nearly_equal(layout->anchors[0].pos.z, 0.0f));
    TEST_ASSERT(layout->anchors[0].handleAxis == VIEW_PLANE_XY);

    shutdown_runtime();
    return true;
}

static bool test_layout_scene_bounds_clamp_contract(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;
    SceneBounds3D bounds = layout->scene3d.bounds;
    Vec3 point = { 3.5f, -4.0f, 0.5f };
    bool clamped = false;

    bounds.enabled = true;
    bounds.min = (Vec3){ -1.0f, -2.0f, -3.0f };
    bounds.max = (Vec3){  1.0f,  2.0f,  3.0f };
    TEST_ASSERT(Layout_SceneBounds3D_IsValid(&bounds));
    TEST_ASSERT(Layout_SceneBounds3D_ClampPoint(&bounds, &point, &clamped));
    TEST_ASSERT(clamped);
    TEST_ASSERT(nearly_equal(point.x, 1.0f));
    TEST_ASSERT(nearly_equal(point.y, -2.0f));
    TEST_ASSERT(nearly_equal(point.z, 0.5f));

    bounds.enabled = false;
    point = (Vec3){ 99.0f, -99.0f, 50.0f };
    clamped = true;
    TEST_ASSERT(Layout_SceneBounds3D_ClampPoint(&bounds, &point, &clamped));
    TEST_ASSERT(!clamped);
    TEST_ASSERT(nearly_equal(point.x, 99.0f));
    TEST_ASSERT(nearly_equal(point.y, -99.0f));
    TEST_ASSERT(nearly_equal(point.z, 50.0f));

    bounds.enabled = true;
    bounds.min.x = 2.0f;
    bounds.max.x = 1.0f;
    TEST_ASSERT(!Layout_SceneBounds3D_IsValid(&bounds));
    TEST_ASSERT(!Layout_SceneBounds3D_ClampPoint(&bounds, &point, NULL));

    shutdown_runtime();
    return true;
}

static bool test_layout_json_v6_persists_scene_bounds(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    layout->scene3d.bounds.enabled = true;
    layout->scene3d.bounds.clampOnEdit = true;
    layout->scene3d.bounds.min = (Vec3){ -7.0f, -8.0f, -9.0f };
    layout->scene3d.bounds.max = (Vec3){ 7.0f, 8.0f, 9.0f };

    char* json = Layout_SaveToString(layout);
    TEST_ASSERT(json != NULL);

    cJSON* root = cJSON_Parse(json);
    TEST_ASSERT(root != NULL);
    const cJSON* file = cJSON_GetObjectItem(root, "file");
    const cJSON* version = cJSON_GetObjectItem(file, "schemaVersion");
    const cJSON* scene3d = cJSON_GetObjectItem(root, "scene3d");
    const cJSON* bounds = cJSON_GetObjectItem(scene3d, "bounds");
    const cJSON* min = cJSON_GetObjectItem(bounds, "min");
    const cJSON* max = cJSON_GetObjectItem(bounds, "max");
    TEST_ASSERT(cJSON_IsNumber(version));
    TEST_ASSERT(version->valueint == LAYOUT_JSON_SCHEMA_VERSION);
    TEST_ASSERT(cJSON_IsObject(scene3d));
    TEST_ASSERT(cJSON_IsObject(bounds));
    TEST_ASSERT(cJSON_IsObject(min));
    TEST_ASSERT(cJSON_IsObject(max));
    TEST_ASSERT(cJSON_IsTrue(cJSON_GetObjectItem(bounds, "enabled")));
    TEST_ASSERT(cJSON_IsTrue(cJSON_GetObjectItem(bounds, "clampOnEdit")));
    TEST_ASSERT(nearly_equal((float)cJSON_GetObjectItem(min, "x")->valuedouble, -7.0f));
    TEST_ASSERT(nearly_equal((float)cJSON_GetObjectItem(max, "z")->valuedouble, 9.0f));
    cJSON_Delete(root);

    TEST_ASSERT(Layout_LoadFromString(layout, json));
    free(json);
    TEST_ASSERT(layout->scene3d.bounds.enabled);
    TEST_ASSERT(layout->scene3d.bounds.clampOnEdit);
    TEST_ASSERT(nearly_equal(layout->scene3d.bounds.min.x, -7.0f));
    TEST_ASSERT(nearly_equal(layout->scene3d.bounds.min.y, -8.0f));
    TEST_ASSERT(nearly_equal(layout->scene3d.bounds.min.z, -9.0f));
    TEST_ASSERT(nearly_equal(layout->scene3d.bounds.max.x, 7.0f));
    TEST_ASSERT(nearly_equal(layout->scene3d.bounds.max.y, 8.0f));
    TEST_ASSERT(nearly_equal(layout->scene3d.bounds.max.z, 9.0f));

    shutdown_runtime();
    return true;
}

static bool test_layout_json_missing_scene3d_defaults_scene_bounds(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;
    Scene3DSettings defaults = {0};
    Layout_Scene3DSettings_SetDefaults(&defaults);

    const char* legacyJsonWithoutScene3d =
        "{"
        "\"file\":{\"schemaVersion\":4,\"gridSize\":1},"
        "\"anchors\":[],"
        "\"walls\":[]"
        "}";

    TEST_ASSERT(Layout_LoadFromString(layout, legacyJsonWithoutScene3d));
    TEST_ASSERT(layout->scene3d.bounds.enabled == defaults.bounds.enabled);
    TEST_ASSERT(layout->scene3d.bounds.clampOnEdit == defaults.bounds.clampOnEdit);
    TEST_ASSERT(nearly_equal(layout->scene3d.bounds.min.x, defaults.bounds.min.x));
    TEST_ASSERT(nearly_equal(layout->scene3d.bounds.min.y, defaults.bounds.min.y));
    TEST_ASSERT(nearly_equal(layout->scene3d.bounds.min.z, defaults.bounds.min.z));
    TEST_ASSERT(nearly_equal(layout->scene3d.bounds.max.x, defaults.bounds.max.x));
    TEST_ASSERT(nearly_equal(layout->scene3d.bounds.max.y, defaults.bounds.max.y));
    TEST_ASSERT(nearly_equal(layout->scene3d.bounds.max.z, defaults.bounds.max.z));

    shutdown_runtime();
    return true;
}

static bool test_construction_plane_axis_mode_maps_to_view_context(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_3D, false));

    ViewPlane legacyPlane = { .axis = VIEW_PLANE_YZ, .offset = 4.25f };
    state->activePlane = (ViewPlane){ .axis = VIEW_PLANE_XY, .offset = 0.0f };
    Layout_ConstructionPlane3D_SetFromViewPlane(&state->layout.scene3d.constructionPlane, legacyPlane);

    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    TEST_ASSERT(viewCtx.plane.axis == VIEW_PLANE_YZ);
    TEST_ASSERT(nearly_equal(viewCtx.plane.offset, 4.25f));

    shutdown_runtime();
    return true;
}

static bool test_construction_plane_custom_frame_validation_and_projection_fallback(void) {
    ConstructionPlane3D plane = {0};
    Layout_ConstructionPlane3D_SetDefaults(&plane);
    plane.mode = CONSTRUCTION_PLANE_MODE_CUSTOM_FRAME;
    plane.customFrame.origin = (Vec3){ 0.0f, 6.5f, 0.0f };
    plane.customFrame.axisU = (Vec3){ 1.0f, 0.0f, 0.0f };
    plane.customFrame.axisV = (Vec3){ 0.0f, 0.0f, 1.0f };
    plane.customFrame.normal = (Vec3){ 0.0f, 1.0f, 0.0f };

    TEST_ASSERT(Layout_ConstructionPlane3D_IsValid(&plane));
    {
        ViewPlane fallback = Layout_ConstructionPlane3D_ToViewPlane(&plane);
        TEST_ASSERT(fallback.axis == VIEW_PLANE_XZ);
        TEST_ASSERT(nearly_equal(fallback.offset, 6.5f));
    }

    plane.customFrame.axisV = plane.customFrame.axisU;
    TEST_ASSERT(!Layout_ConstructionPlane3D_IsValid(&plane));

    return true;
}

static bool test_layout_json_v6_persists_construction_plane_custom_frame(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    layout->scene3d.constructionPlane.mode = CONSTRUCTION_PLANE_MODE_CUSTOM_FRAME;
    layout->scene3d.constructionPlane.customFrame.origin = (Vec3){ 3.5f, 0.0f, 0.0f };
    layout->scene3d.constructionPlane.customFrame.axisU = (Vec3){ 0.0f, 1.0f, 0.0f };
    layout->scene3d.constructionPlane.customFrame.axisV = (Vec3){ 0.0f, 0.0f, 1.0f };
    layout->scene3d.constructionPlane.customFrame.normal = (Vec3){ 1.0f, 0.0f, 0.0f };
    TEST_ASSERT(Layout_ConstructionPlane3D_IsValid(&layout->scene3d.constructionPlane));

    char* json = Layout_SaveToString(layout);
    TEST_ASSERT(json != NULL);
    cJSON* root = cJSON_Parse(json);
    TEST_ASSERT(root != NULL);
    {
        const cJSON* scene3d = cJSON_GetObjectItem(root, "scene3d");
        const cJSON* cp = cJSON_GetObjectItem(scene3d, "constructionPlane");
        const cJSON* mode = cJSON_GetObjectItem(cp, "mode");
        const cJSON* frame = cJSON_GetObjectItem(cp, "customFrame");
        const cJSON* origin = cJSON_GetObjectItem(frame, "origin");
        TEST_ASSERT(cJSON_IsString(mode));
        TEST_ASSERT(strcmp(mode->valuestring, "custom_frame") == 0);
        TEST_ASSERT(cJSON_IsObject(origin));
        TEST_ASSERT(nearly_equal((float)cJSON_GetObjectItem(origin, "x")->valuedouble, 3.5f));
    }
    cJSON_Delete(root);

    TEST_ASSERT(Layout_LoadFromString(layout, json));
    free(json);
    TEST_ASSERT(layout->scene3d.constructionPlane.mode == CONSTRUCTION_PLANE_MODE_CUSTOM_FRAME);
    TEST_ASSERT(Layout_ConstructionPlane3D_IsValid(&layout->scene3d.constructionPlane));
    {
        ViewPlane fallback = Layout_ConstructionPlane3D_ToViewPlane(&layout->scene3d.constructionPlane);
        TEST_ASSERT(fallback.axis == VIEW_PLANE_YZ);
        TEST_ASSERT(nearly_equal(fallback.offset, 3.5f));
    }

    shutdown_runtime();
    return true;
}

static bool test_object_store_id_stability_and_tombstone_delete(void) {
    init_runtime();
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

    shutdown_runtime();
    return true;
}

static bool test_object_store_plane_lock_dimensional_rules_enforced(void) {
    init_runtime();
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
    TEST_ASSERT(nearly_equal(object->transform.position.x, 0.0f));
    TEST_ASSERT(nearly_equal(object->transform.position.y, 2.0f));
    TEST_ASSERT(nearly_equal(object->transform.position.z, 1.0f));

    shutdown_runtime();
    return true;
}

static bool test_object_store_rejects_invalid_transform_scale(void) {
    init_runtime();
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

    shutdown_runtime();
    return true;
}

static bool test_plane_primitive_creation_respects_bounds_and_construction_plane(void) {
    init_runtime();
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
    TEST_ASSERT(nearly_equal(object->transform.position.x, 0.0f));
    TEST_ASSERT(nearly_equal(object->transform.position.y, 0.0f));
    TEST_ASSERT(nearly_equal(object->transform.position.z, 2.0f));
    TEST_ASSERT(nearly_equal(object->plane.width, 4.0f));
    TEST_ASSERT(nearly_equal(object->plane.height, 4.0f));

    shutdown_runtime();
    return true;
}

static bool test_rect_prism_primitive_creation_respects_bounds_and_construction_plane(void) {
    init_runtime();
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
    TEST_ASSERT(nearly_equal(object->transform.position.x, 0.0f));
    TEST_ASSERT(nearly_equal(object->transform.position.y, 0.0f));
    TEST_ASSERT(nearly_equal(object->transform.position.z, 0.0f));
    TEST_ASSERT(nearly_equal(object->rectPrism.width, 4.0f));
    TEST_ASSERT(nearly_equal(object->rectPrism.height, 4.0f));
    TEST_ASSERT(nearly_equal(object->rectPrism.depth, 4.0f));
    TEST_ASSERT(vec3_nearly_equal(object->rectPrism.frame.origin, object->transform.position));

    shutdown_runtime();
    return true;
}

static bool test_rect_prism_primitive_creation_clamps_depth_to_zero_at_bounds_limit(void) {
    init_runtime();
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
    TEST_ASSERT(nearly_equal(object->rectPrism.width, 1.0f));
    TEST_ASSERT(nearly_equal(object->rectPrism.height, 1.0f));
    TEST_ASSERT(nearly_equal(object->rectPrism.depth, 0.0f));
    TEST_ASSERT(nearly_equal(object->transform.position.z, 2.0f));

    shutdown_runtime();
    return true;
}

static bool test_layout_object3d_compute_rect_prism_corners_contract(void) {
    init_runtime();
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
    TEST_ASSERT(nearly_equal(Vec3_Distance(corners[0], corners[1]), 2.0f));
    TEST_ASSERT(nearly_equal(Vec3_Distance(corners[1], corners[2]), 4.0f));
    TEST_ASSERT(nearly_equal(Vec3_Distance(corners[0], corners[4]), 6.0f));

    shutdown_runtime();
    return true;
}

static bool test_layout_json_v8_persists_plane_primitives_deterministically(void) {
    init_runtime();
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
    TEST_ASSERT(nearly_equal(loaded1->plane.width, 2.5f));
    TEST_ASSERT(nearly_equal(loaded2->plane.height, 3.5f));

    {
        uint32_t id3 = 0u;
        layout->scene3d.constructionPlane.axisAligned = (ViewPlane){ .axis = VIEW_PLANE_XZ, .offset = 0.5f };
        TEST_ASSERT(Layout_CreatePlanePrimitive(layout, &params, &id3, &adjusted));
        TEST_ASSERT(id3 == 3u);
    }

    free(first);
    free(second);
    shutdown_runtime();
    return true;
}

static bool test_object_store_rect_prism_payload_validation(void) {
    init_runtime();
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

    shutdown_runtime();
    return true;
}

static bool test_layout_json_v8_persists_rect_prism_payload_deterministically(void) {
    init_runtime();
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
        TEST_ASSERT(nearly_equal((float)cJSON_GetObjectItem(rp0, "width")->valuedouble, 2.0f));
        TEST_ASSERT(nearly_equal((float)cJSON_GetObjectItem(rp0, "depth")->valuedouble, 4.0f));
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
    TEST_ASSERT(nearly_equal(loaded1->rectPrism.width, 2.0f));
    TEST_ASSERT(nearly_equal(loaded1->rectPrism.height, 3.0f));
    TEST_ASSERT(nearly_equal(loaded1->rectPrism.depth, 4.0f));
    TEST_ASSERT(loaded1->rectPrism.lockToBounds);
    TEST_ASSERT(nearly_equal(loaded1->transform.rotationDeg.y, 32.5f));
    TEST_ASSERT(nearly_equal(loaded2->rectPrism.width, 1.5f));
    TEST_ASSERT(nearly_equal(loaded2->rectPrism.height, 2.5f));
    TEST_ASSERT(nearly_equal(loaded2->rectPrism.depth, 0.75f));
    TEST_ASSERT(loaded2->rectPrism.lockToConstructionPlane);

    free(first);
    free(second);
    shutdown_runtime();
    return true;
}

static bool test_hitbox_plane_object_is_selectable(void) {
    init_runtime();
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

    shutdown_runtime();
    return true;
}

static bool test_plane_resize_corner_drag_updates_size_and_center(void) {
    init_runtime();
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
    TEST_ASSERT(nearly_equal(object->plane.width, movedU - fixedU));
    TEST_ASSERT(nearly_equal(object->plane.height, movedV - fixedV));
    {
        Vec3 expectedCenter = Vec3_Add(startCenter,
                                       Vec3_Add(Vec3_Scale(axisU, (fixedU + movedU) * 0.5f),
                                                Vec3_Scale(axisV, (fixedV + movedV) * 0.5f)));
        TEST_ASSERT(vec3_nearly_equal(object->transform.position, expectedCenter));
    }

    shutdown_runtime();
    return true;
}

static bool test_plane_resize_edge_drag_keeps_opposite_edge_fixed(void) {
    init_runtime();
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
    TEST_ASSERT(nearly_equal(object->plane.width, movedU - fixedU));
    TEST_ASSERT(nearly_equal(object->plane.height, 4.0f));
    {
        Vec3 expectedCenter = Vec3_Add(startCenter, Vec3_Scale(axisU, (fixedU + movedU) * 0.5f));
        TEST_ASSERT(vec3_nearly_equal(object->transform.position, expectedCenter));
    }

    shutdown_runtime();
    return true;
}

static bool test_plane_resize_edge_drag_clamps_to_min_size(void) {
    init_runtime();
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
    TEST_ASSERT(nearly_equal(object->plane.height, 4.0f));
    {
        const float movedU = fixedU - object->plane.width;
        Vec3 expectedCenter = Vec3_Add(startCenter, Vec3_Scale(axisU, (fixedU + movedU) * 0.5f));
        TEST_ASSERT(vec3_nearly_equal(object->transform.position, expectedCenter));
    }

    shutdown_runtime();
    return true;
}

static bool test_plane_resize_corner_drag_crosses_anchor_and_flips_axes(void) {
    init_runtime();
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
    TEST_ASSERT(nearly_equal(object->plane.width, fabsf(movedU - fixedU)));
    TEST_ASSERT(nearly_equal(object->plane.height, fabsf(movedV - fixedV)));
    {
        Vec3 expectedCenter = Vec3_Add(startCenter,
                                       Vec3_Add(Vec3_Scale(axisU, (fixedU + movedU) * 0.5f),
                                                Vec3_Scale(axisV, (fixedV + movedV) * 0.5f)));
        TEST_ASSERT(vec3_nearly_equal(object->transform.position, expectedCenter));
    }

    shutdown_runtime();
    return true;
}

static bool test_plane_resize_handle_resolution_flips_corner_axes(void) {
    init_runtime();
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

    shutdown_runtime();
    return true;
}

static bool test_plane_resize_handle_resolution_flips_edge_axis(void) {
    init_runtime();
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

    shutdown_runtime();
    return true;
}

static bool test_rect_prism_resize_corner_drag_updates_size_and_center(void) {
    init_runtime();
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
    TEST_ASSERT(nearly_equal(object->rectPrism.width, movedU - fixedU));
    TEST_ASSERT(nearly_equal(object->rectPrism.height, movedV - fixedV));
    TEST_ASSERT(nearly_equal(object->rectPrism.depth, 6.0f));
    {
        Vec3 expectedCenter = Vec3_Add(startCenter,
                                       Vec3_Add(Vec3_Scale(axisU, (fixedU + movedU) * 0.5f),
                                                Vec3_Scale(axisV, (fixedV + movedV) * 0.5f)));
        TEST_ASSERT(vec3_nearly_equal(object->transform.position, expectedCenter));
    }

    shutdown_runtime();
    return true;
}

static bool test_rect_prism_resize_handle_resolution_flips_corner_axes(void) {
    init_runtime();
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

    shutdown_runtime();
    return true;
}

static bool test_rect_prism_depth_resize_from_face_handle_updates_depth_and_center(void) {
    init_runtime();
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
    TEST_ASSERT(nearly_equal(object->rectPrism.width, 4.0f));
    TEST_ASSERT(nearly_equal(object->rectPrism.height, 4.0f));
    TEST_ASSERT(nearly_equal(object->rectPrism.depth, movedN - fixedN));
    {
        Vec3 expectedCenter = Vec3_Add(startCenter, Vec3_Scale(axisN, (fixedN + movedN) * 0.5f));
        TEST_ASSERT(vec3_nearly_equal(object->transform.position, expectedCenter));
    }

    shutdown_runtime();
    return true;
}

static bool test_rect_prism_set_dimensions_preserves_center_and_updates_size(void) {
    init_runtime();
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
    TEST_ASSERT(nearly_equal(object->rectPrism.width, 9.0f));
    TEST_ASSERT(nearly_equal(object->rectPrism.height, 8.0f));
    TEST_ASSERT(nearly_equal(object->rectPrism.depth, 0.0f));
    TEST_ASSERT(vec3_nearly_equal(object->transform.position, centerBefore));
    TEST_ASSERT(Layout_ObjectStore_ValidateObject(object));

    shutdown_runtime();
    return true;
}

static bool test_rect_prism_set_dimensions_clamps_to_bounds_when_locked(void) {
    init_runtime();
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
    TEST_ASSERT(nearly_equal(object->rectPrism.width, 4.0f));
    TEST_ASSERT(nearly_equal(object->rectPrism.height, 4.0f));
    TEST_ASSERT(nearly_equal(object->rectPrism.depth, 4.0f));
    TEST_ASSERT(Layout_ObjectStore_ValidateObject(object));

    shutdown_runtime();
    return true;
}

static bool test_rect_prism_set_dimensions_rejects_invalid_values(void) {
    init_runtime();
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

    shutdown_runtime();
    return true;
}

static bool test_layout_set_object3d_position_moves_plane_and_preserves_dimensions(void) {
    init_runtime();
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
    TEST_ASSERT(vec3_nearly_equal(object->transform.position, (Vec3){ 7.0f, -3.0f, 4.0f }));
    TEST_ASSERT(vec3_nearly_equal(object->plane.frame.origin, object->transform.position));
    TEST_ASSERT(nearly_equal(object->plane.width, 6.0f));
    TEST_ASSERT(nearly_equal(object->plane.height, 2.0f));

    shutdown_runtime();
    return true;
}

static bool test_layout_set_object3d_position_clamps_locked_prism_to_bounds(void) {
    init_runtime();
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
    TEST_ASSERT(vec3_nearly_equal(object->rectPrism.frame.origin, object->transform.position));
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

    shutdown_runtime();
    return true;
}

static bool test_layout_rotate_object3d_rotates_plane_frame_and_preserves_dimensions(void) {
    init_runtime();
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

    TEST_ASSERT(nearly_equal(object->plane.width, 6.0f));
    TEST_ASSERT(nearly_equal(object->plane.height, 2.0f));
    TEST_ASSERT(vec3_nearly_equal(object->plane.frame.origin, object->transform.position));
    TEST_ASSERT(fabsf(Vec3_Dot(startU, nextU)) < 0.01f);
    TEST_ASSERT(fabsf(Vec3_Dot(startV, nextV)) < 0.01f);
    TEST_ASSERT(fabsf(Vec3_Dot(startN, nextN)) > 0.99f);
    TEST_ASSERT(fabsf(Vec3_Dot(nextU, nextV)) < 0.01f);
    TEST_ASSERT(fabsf(Vec3_Dot(nextU, nextN)) < 0.01f);
    TEST_ASSERT(fabsf(Vec3_Dot(nextV, nextN)) < 0.01f);
    TEST_ASSERT(nearly_equal(object->transform.rotationDeg.z, 90.0f));

    shutdown_runtime();
    return true;
}

static bool test_layout_rotate_object3d_baseline_composition_is_deterministic(void) {
    init_runtime();
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

    TEST_ASSERT(vec3_nearly_equal(objectA->rectPrism.frame.axisU, objectB->rectPrism.frame.axisU));
    TEST_ASSERT(vec3_nearly_equal(objectA->rectPrism.frame.axisV, objectB->rectPrism.frame.axisV));
    TEST_ASSERT(vec3_nearly_equal(objectA->rectPrism.frame.normal, objectB->rectPrism.frame.normal));
    TEST_ASSERT(vec3_nearly_equal(objectA->transform.rotationDeg, objectB->transform.rotationDeg));

    shutdown_runtime();
    return true;
}

static bool test_layout_rotate_object3d_clamps_locked_bounds(void) {
    init_runtime();
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

    shutdown_runtime();
    return true;
}

static bool test_ui_panel_display_unit_conversion_roundtrip(void) {
    init_runtime();

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

    shutdown_runtime();
    return true;
}

static bool test_ui_panel_display_unit_rounding_determinism(void) {
    init_runtime();
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

    shutdown_runtime();
    return true;
}

static bool test_hitbox_plane_resize_handles_emit_for_selected_plane(void) {
    init_runtime();
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

    shutdown_runtime();
    return true;
}

static bool test_hitbox_rect_prism_object_is_selectable(void) {
    init_runtime();
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

    shutdown_runtime();
    return true;
}

static bool test_hitbox_rect_prism_resize_handles_emit_for_selected_prism(void) {
    init_runtime();
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

    shutdown_runtime();
    return true;
}

static bool test_hitbox_rect_prism_gizmo_axes_emit_for_selected_handle_in_free_view(void) {
    init_runtime();
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

    shutdown_runtime();
    return true;
}

static bool test_hitbox_object3d_center_gizmo_axes_emit_for_selected_object_in_free_view(void) {
    init_runtime();
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

    shutdown_runtime();
    return true;
}

static bool test_hitbox_object3d_center_gizmo_hidden_when_prism_handle_selected(void) {
    init_runtime();
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

    shutdown_runtime();
    return true;
}

static bool test_hitbox_rect_prism_free_view_emits_all_corner_and_edge_handles(void) {
    init_runtime();
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

    shutdown_runtime();
    return true;
}

static bool test_rect_prism_3d_handle_resize_allows_zero_depth_and_reexpand(void) {
    init_runtime();
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
    TEST_ASSERT(nearly_equal(object->rectPrism.depth, 0.0f));

    Vec3 collapsedHandleWorld = {0};
    TEST_ASSERT(Layout_RectPrismResizeHandleWorldPoint(object,
                                                       RECT_PRISM_RESIZE_HANDLE_CORNER_6,
                                                       &collapsedHandleWorld));

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
    TEST_ASSERT(nearly_equal(object->rectPrism.depth, 3.0f));

    shutdown_runtime();
    return true;
}

static bool test_layout_json_preserves_anchor_handles(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    int idx = Layout_AddAnchor(layout, (Vec2){ 1.0f, 2.0f });
    Anchor* anchor = &layout->anchors[idx];
    anchor->type = ANCHOR_TYPE_CURVE;
    anchor->handlesLinked = false;
    anchor->handleAxis = VIEW_PLANE_YZ;
    anchor->handleInLength = 2.5f;
    anchor->handleInAngleDeg = 45.0f;
    anchor->handleOutLength = 1.25f;
    anchor->handleOutAngleDeg = -60.0f;

    char* snapshot = Layout_SaveToString(layout);
    TEST_ASSERT(snapshot != NULL);

    TEST_ASSERT(Layout_LoadFromString(layout, snapshot));
    free(snapshot);

    TEST_ASSERT(layout->anchorCount == 1);
    Anchor* loaded = &layout->anchors[0];
    TEST_ASSERT(loaded->type == ANCHOR_TYPE_CURVE);
    TEST_ASSERT(!loaded->handlesLinked);
    TEST_ASSERT(loaded->handleAxis == VIEW_PLANE_YZ);
    TEST_ASSERT(nearly_equal(loaded->handleInLength, 2.5f));
    TEST_ASSERT(nearly_equal(loaded->handleInAngleDeg, 45.0f));
    TEST_ASSERT(nearly_equal(loaded->handleOutLength, 1.25f));
    TEST_ASSERT(nearly_equal(loaded->handleOutAngleDeg, -60.0f));

    shutdown_runtime();
    return true;
}

static bool test_layout_json_v3_persists_anchor_z(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    int idx = Layout_AddAnchor3(layout, (Vec3){ 1.0f, 2.0f, 7.25f });
    TEST_ASSERT(idx >= 0);

    char* snapshot = Layout_SaveToString(layout);
    TEST_ASSERT(snapshot != NULL);
    TEST_ASSERT(Layout_LoadFromString(layout, snapshot));
    free(snapshot);

    TEST_ASSERT(layout->anchorCount == 1);
    TEST_ASSERT(nearly_equal(layout->anchors[0].pos.x, 1.0f));
    TEST_ASSERT(nearly_equal(layout->anchors[0].pos.y, 2.0f));
    TEST_ASSERT(nearly_equal(layout->anchors[0].pos.z, 7.25f));

    shutdown_runtime();
    return true;
}

static bool test_layout_json_v2_defaults_z_to_zero(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    const char* v2Json =
        "{"
        "\"file\":{\"schemaVersion\":2,\"gridSize\":1},"
        "\"anchors\":[{\"x\":3,\"y\":4,\"persistent\":false}],"
        "\"walls\":[]"
        "}";

    TEST_ASSERT(Layout_LoadFromString(layout, v2Json));
    TEST_ASSERT(layout->anchorCount == 1);
    TEST_ASSERT(nearly_equal(layout->anchors[0].pos.x, 3.0f));
    TEST_ASSERT(nearly_equal(layout->anchors[0].pos.y, 4.0f));
    TEST_ASSERT(nearly_equal(layout->anchors[0].pos.z, 0.0f));

    shutdown_runtime();
    return true;
}

static bool test_layout_json_v4_defaults_z_to_zero_when_omitted(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    const char* v4JsonWithoutZ =
        "{"
        "\"file\":{\"schemaVersion\":4,\"gridSize\":2},"
        "\"anchors\":[{\"x\":9,\"y\":-3,\"persistent\":true,\"futureTag\":\"keep_ignored\"}],"
        "\"walls\":[]"
        "}";

    TEST_ASSERT(Layout_LoadFromString(layout, v4JsonWithoutZ));
    TEST_ASSERT(layout->anchorCount == 1);
    TEST_ASSERT(nearly_equal(layout->gridSize, 2.0f));
    TEST_ASSERT(nearly_equal(layout->anchors[0].pos.x, 9.0f));
    TEST_ASSERT(nearly_equal(layout->anchors[0].pos.y, -3.0f));
    TEST_ASSERT(nearly_equal(layout->anchors[0].pos.z, 0.0f));

    shutdown_runtime();
    return true;
}

static bool test_layout_json_accepts_additive_unknown_fields(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    const char* additiveJson =
        "{"
        "\"file\":{\"schemaVersion\":4,\"gridSize\":1.5,\"futureFlag\":true},"
        "\"scene\":{\"space\":\"3d\",\"up\":\"z\"},"
        "\"anchors\":["
        "{\"x\":0,\"y\":1,\"z\":2,\"persistent\":false,\"type\":\"curve\","
        "\"handlesLinked\":false,\"handleAxis\":\"xz\","
        "\"handleInLength\":1.25,\"handleInAngleDeg\":15,"
        "\"handleOutLength\":2.5,\"handleOutAngleDeg\":-75,"
        "\"futureAnchorField\":\"ok\"},"
        "{\"x\":4,\"y\":5,\"z\":6,\"persistent\":true}"
        "],"
        "\"walls\":[{\"a\":0,\"b\":1,\"futureWallField\":7}]"
        "}";

    TEST_ASSERT(Layout_LoadFromString(layout, additiveJson));
    TEST_ASSERT(layout->anchorCount == 2);
    TEST_ASSERT(layout->wallCount == 1);
    TEST_ASSERT(nearly_equal(layout->gridSize, 1.5f));
    TEST_ASSERT(nearly_equal(layout->anchors[0].pos.z, 2.0f));
    TEST_ASSERT(layout->anchors[0].type == ANCHOR_TYPE_CURVE);
    TEST_ASSERT(layout->anchors[0].handleAxis == VIEW_PLANE_XZ);
    TEST_ASSERT(nearly_equal(layout->anchors[0].handleInLength, 1.25f));
    TEST_ASSERT(nearly_equal(layout->anchors[0].handleOutAngleDeg, -75.0f));
    TEST_ASSERT(layout->anchors[1].isPersistent == true);

    shutdown_runtime();
    return true;
}

static bool test_layout_handles_link_toggle(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    int idx = Layout_AddAnchor(layout, (Vec2){ 0.0f, 0.0f });
    Anchor* anchor = &layout->anchors[idx];
    anchor->type = ANCHOR_TYPE_CURVE;
    anchor->handlesLinked = false;
    anchor->handleInLength = 1.0f;
    anchor->handleInAngleDeg = 30.0f;
    anchor->handleOutLength = 2.0f;
    anchor->handleOutAngleDeg = 120.0f;

    TEST_ASSERT(Layout_SetHandlesLinked(layout, idx, true));
    TEST_ASSERT(anchor->handlesLinked);
    TEST_ASSERT(nearly_equal(anchor->handleInLength, 2.0f));
    TEST_ASSERT(nearly_equal(anchor->handleOutLength, 2.0f));
    TEST_ASSERT(nearly_equal(anchor->handleOutAngleDeg,
                             Angle_NormalizeDeg(anchor->handleInAngleDeg + 180.0f)));

    TEST_ASSERT(Layout_SetHandlesLinked(layout, idx, false));
    TEST_ASSERT(anchor->handlesLinked == false);

    shutdown_runtime();
    return true;
}

static bool test_editor_history_limit_enforced(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;
    EditorState* editor = &state->editor;

    for (size_t i = 0; i < EDITOR_HISTORY_MAX + 8; ++i) {
        Layout_AddWall(layout,
                       (Vec2){ (float)i, 0.0f },
                       (Vec2){ (float)i + 1.0f, 0.0f });
        Editor_HistoryCapture(editor, layout);
    }

    TEST_ASSERT(Editor_UndoCount(editor) == EDITOR_HISTORY_MAX);
    TEST_ASSERT(Editor_RedoCount(editor) == 0);

    shutdown_runtime();
    return true;
}

static bool test_layout_add_anchor3_preserves_z(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    int idx = Layout_AddAnchor3(layout, (Vec3){ 1.0f, 2.0f, 3.5f });
    TEST_ASSERT(idx >= 0);
    TEST_ASSERT(layout->anchorCount == 1);
    TEST_ASSERT(nearly_equal(layout->anchors[0].pos.x, 1.0f));
    TEST_ASSERT(nearly_equal(layout->anchors[0].pos.y, 2.0f));
    TEST_ASSERT(nearly_equal(layout->anchors[0].pos.z, 3.5f));

    shutdown_runtime();
    return true;
}

static bool test_layout_corner_anchor_allows_more_than_two_connections(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    Vec3 center = { 0.0f, 0.0f, 0.0f };
    Layout_AddWall3(layout, center, (Vec3){ 1.0f, 0.0f, 0.0f });
    Layout_AddWall3(layout, center, (Vec3){ 0.0f, 1.0f, 0.0f });
    Layout_AddWall3(layout, center, (Vec3){ -1.0f, 0.0f, 0.0f });

    int centerIdx = Layout_AddAnchor3(layout, center);
    TEST_ASSERT(centerIdx >= 0);
    TEST_ASSERT(layout->anchors[centerIdx].connectionCount == 3);

    shutdown_runtime();
    return true;
}

static bool test_hitbox_prefers_nearer_plane_depth_for_overlapping_points(void) {
    init_runtime();
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

    shutdown_runtime();
    return true;
}

static bool test_hitbox_gizmo_axis_emits_for_selected_anchor_in_free_view(void) {
    init_runtime();
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

    shutdown_runtime();
    return true;
}

static bool test_hitbox_gizmo_axis_disabled_when_free_view_off(void) {
    init_runtime();
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

    shutdown_runtime();
    return true;
}

static bool test_layout_compute_centroid_ignores_deleted_anchors(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    int a = Layout_AddAnchor3(layout, (Vec3){ 0.0f, 0.0f, 0.0f });
    int b = Layout_AddAnchor3(layout, (Vec3){ 10.0f, 2.0f, 4.0f });
    int c = Layout_AddAnchor3(layout, (Vec3){ 20.0f, 6.0f, 8.0f });
    TEST_ASSERT(a >= 0 && b >= 0 && c >= 0);

    Layout_MarkAnchorDeleted(layout, c);
    bool hasAnchors = false;
    Vec3 center = Layout_ComputeCentroid(layout, &hasAnchors);
    TEST_ASSERT(hasAnchors);
    TEST_ASSERT(nearly_equal(center.x, 5.0f));
    TEST_ASSERT(nearly_equal(center.y, 1.0f));
    TEST_ASSERT(nearly_equal(center.z, 2.0f));

    shutdown_runtime();
    return true;
}

static bool test_shape_export_projection_axis_mapping(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    Layout_AddWall3(layout, (Vec3){ 2.0f, 1.0f, 3.0f }, (Vec3){ 2.0f, 4.0f, 5.0f });

    ShapeDocument doc = {0};
    TEST_ASSERT(ShapeDocument_FromLayoutProjected("plane_yz", layout, VIEW_PLANE_YZ, &doc));
    TEST_ASSERT(doc.shapeCount == 1);
    TEST_ASSERT(doc.shapes[0].pathCount == 1);
    TEST_ASSERT(doc.shapes[0].paths[0].segmentCount == 1);

    const ShapeSegment* seg = &doc.shapes[0].paths[0].segments[0];
    TEST_ASSERT(nearly_equal(seg->p0.x, 1.0f));
    TEST_ASSERT(nearly_equal(seg->p0.y, 3.0f));
    TEST_ASSERT(nearly_equal(seg->p1.x, 4.0f));
    TEST_ASSERT(nearly_equal(seg->p1.y, 5.0f));

    ShapeDocument_Free(&doc);
    shutdown_runtime();
    return true;
}

static bool test_space_mode_toggle_contract_2d_resets_plane_and_free_view(void) {
    init_runtime();
    GlobalState* state = Global_Get();

    state->activePlane.axis = VIEW_PLANE_YZ;
    state->activePlane.offset = 9.0f;
    state->freeViewCamera.enabled = true;

    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_2D, false));
    TEST_ASSERT(state->spaceMode == SPACE_MODE_2D);
    TEST_ASSERT(state->activePlane.axis == VIEW_PLANE_XY);
    TEST_ASSERT(nearly_equal(state->activePlane.offset, 0.0f));
    TEST_ASSERT(!state->freeViewCamera.enabled);

    TEST_ASSERT(Global_ToggleSpaceMode(false));
    TEST_ASSERT(state->spaceMode == SPACE_MODE_3D);
    TEST_ASSERT(state->activePlane.axis == VIEW_PLANE_XY);
    TEST_ASSERT(nearly_equal(state->activePlane.offset, 0.0f));
    TEST_ASSERT(!state->freeViewCamera.enabled);

    shutdown_runtime();
    return true;
}

static bool test_canonical_scene_export_2d_payload(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    Layout_AddWall(layout, (Vec2){ 0.0f, 0.0f }, (Vec2){ 2.0f, 1.0f });

    char* scene_json = LineDrawingCanonicalScene_ExportLayoutToString(layout, "scene_test_2d");
    TEST_ASSERT(scene_json != NULL);

    cJSON* root = cJSON_Parse(scene_json);
    TEST_ASSERT(root != NULL);
    free(scene_json);

    {
        cJSON* schema_family = cJSON_GetObjectItem(root, "schema_family");
        cJSON* schema_variant = cJSON_GetObjectItem(root, "schema_variant");
        cJSON* scene_id = cJSON_GetObjectItem(root, "scene_id");
        cJSON* space_mode_intent = cJSON_GetObjectItem(root, "space_mode_intent");
        cJSON* space_mode_default = cJSON_GetObjectItem(root, "space_mode_default");
        cJSON* conversion_policy = cJSON_GetObjectItem(root, "conversion_policy");
        cJSON* unit_system = cJSON_GetObjectItem(root, "unit_system");
        TEST_ASSERT(cJSON_IsString(schema_family));
        TEST_ASSERT(cJSON_IsString(schema_variant));
        TEST_ASSERT(cJSON_IsString(scene_id));
        TEST_ASSERT(cJSON_IsString(space_mode_intent));
        TEST_ASSERT(cJSON_IsString(space_mode_default));
        TEST_ASSERT(cJSON_IsString(conversion_policy));
        TEST_ASSERT(cJSON_IsString(unit_system));
        TEST_ASSERT(strcmp(schema_family->valuestring, "codework_scene") == 0);
        TEST_ASSERT(strcmp(schema_variant->valuestring, "scene_authoring_v1") == 0);
        TEST_ASSERT(strcmp(scene_id->valuestring, "scene_test_2d") == 0);
        TEST_ASSERT(strcmp(space_mode_intent->valuestring, "2d") == 0);
        TEST_ASSERT(strcmp(space_mode_default->valuestring, "2d") == 0);
        TEST_ASSERT(strcmp(conversion_policy->valuestring, "explicit_only") == 0);
        TEST_ASSERT(strcmp(unit_system->valuestring, "meters") == 0);
    }

    cJSON* objects = cJSON_GetObjectItem(root, "objects");
    cJSON* hierarchy = cJSON_GetObjectItem(root, "hierarchy");
    cJSON* materials = cJSON_GetObjectItem(root, "materials");
    cJSON* lights = cJSON_GetObjectItem(root, "lights");
    cJSON* cameras = cJSON_GetObjectItem(root, "cameras");
    TEST_ASSERT(cJSON_IsArray(objects));
    TEST_ASSERT(cJSON_IsArray(hierarchy));
    TEST_ASSERT(cJSON_IsArray(materials));
    TEST_ASSERT(cJSON_IsArray(lights));
    TEST_ASSERT(cJSON_IsArray(cameras));
    TEST_ASSERT(cJSON_GetArraySize(objects) == 3);

    cJSON* obj = find_object_by_id(objects, "obj_line_drawing_layout");
    TEST_ASSERT(cJSON_IsObject(obj));
    {
        cJSON* object_mode_intent = cJSON_GetObjectItem(obj, "space_mode_intent");
        cJSON* object_id = cJSON_GetObjectItem(obj, "object_id");
        cJSON* dimensional_mode = cJSON_GetObjectItem(obj, "dimensional_mode");
        cJSON* locked_plane = cJSON_GetObjectItem(obj, "locked_plane");
        cJSON* projection_policy = cJSON_GetObjectItem(obj, "projection_policy");
        cJSON* extrusion_policy = cJSON_GetObjectItem(obj, "extrusion_policy");
        cJSON* material_ref = cJSON_GetObjectItem(obj, "material_ref");
        TEST_ASSERT(cJSON_IsString(object_mode_intent));
        TEST_ASSERT(cJSON_IsString(object_id));
        TEST_ASSERT(cJSON_IsString(dimensional_mode));
        TEST_ASSERT(cJSON_IsString(locked_plane));
        TEST_ASSERT(cJSON_IsString(projection_policy));
        TEST_ASSERT(cJSON_IsString(extrusion_policy));
        TEST_ASSERT(cJSON_IsObject(material_ref));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(material_ref, "id")->valuestring, "mat_line_drawing_default") == 0);
        TEST_ASSERT(strcmp(object_mode_intent->valuestring, "2d") == 0);
        TEST_ASSERT(strcmp(object_id->valuestring, "obj_line_drawing_layout") == 0);
        TEST_ASSERT(strcmp(dimensional_mode->valuestring, "plane_locked") == 0);
        TEST_ASSERT(strcmp(locked_plane->valuestring, "xy") == 0);
        TEST_ASSERT(strcmp(projection_policy->valuestring, "plane_xy_lock") == 0);
        TEST_ASSERT(strcmp(extrusion_policy->valuestring, "none") == 0);
    }

    cJSON* transform = cJSON_GetObjectItem(obj, "transform");
    cJSON* position = cJSON_GetObjectItem(transform, "position");
    TEST_ASSERT(nearly_equal((float)cJSON_GetObjectItem(position, "z")->valuedouble, 0.0f));

    {
        cJSON* hierarchy_item = cJSON_GetArrayItem(hierarchy, 0);
        TEST_ASSERT(cJSON_IsObject(hierarchy_item));
        TEST_ASSERT(cJSON_GetArraySize(hierarchy) == 2);
        TEST_ASSERT(cJSON_GetArraySize(materials) == 1);
        TEST_ASSERT(cJSON_GetArraySize(lights) == 1);
        TEST_ASSERT(cJSON_GetArraySize(cameras) == 1);
    }

    TEST_ASSERT(find_object_by_id(objects, "obj_line_drawing_anchor_set") != NULL);
    TEST_ASSERT(find_object_by_id(objects, "obj_line_drawing_wall_set") != NULL);

    cJSON_Delete(root);
    shutdown_runtime();
    return true;
}

static bool test_canonical_scene_export_3d_payload(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    Layout_AddWall3(layout, (Vec3){ 0.0f, 0.0f, 0.0f }, (Vec3){ 0.0f, 1.0f, 5.0f });

    char* scene_json = LineDrawingCanonicalScene_ExportLayoutToString(layout, "scene_test_3d");
    TEST_ASSERT(scene_json != NULL);

    cJSON* root = cJSON_Parse(scene_json);
    TEST_ASSERT(root != NULL);
    free(scene_json);

    {
        cJSON* space_mode_intent = cJSON_GetObjectItem(root, "space_mode_intent");
        cJSON* space_mode_default = cJSON_GetObjectItem(root, "space_mode_default");
        cJSON* conversion_policy = cJSON_GetObjectItem(root, "conversion_policy");
        TEST_ASSERT(cJSON_IsString(space_mode_intent));
        TEST_ASSERT(cJSON_IsString(space_mode_default));
        TEST_ASSERT(cJSON_IsString(conversion_policy));
        TEST_ASSERT(strcmp(space_mode_intent->valuestring, "3d") == 0);
        TEST_ASSERT(strcmp(space_mode_default->valuestring, "3d") == 0);
        TEST_ASSERT(strcmp(conversion_policy->valuestring, "explicit_only") == 0);
    }

    cJSON* objects = cJSON_GetObjectItem(root, "objects");
    TEST_ASSERT(cJSON_IsArray(objects));
    TEST_ASSERT(cJSON_GetArraySize(objects) == 3);

    cJSON* obj = find_object_by_id(objects, "obj_line_drawing_layout");
    TEST_ASSERT(cJSON_IsObject(obj));
    {
        cJSON* object_mode_intent = cJSON_GetObjectItem(obj, "space_mode_intent");
        cJSON* dimensional_mode = cJSON_GetObjectItem(obj, "dimensional_mode");
        cJSON* projection_policy = cJSON_GetObjectItem(obj, "projection_policy");
        cJSON* extrusion_policy = cJSON_GetObjectItem(obj, "extrusion_policy");
        TEST_ASSERT(cJSON_IsString(object_mode_intent));
        TEST_ASSERT(cJSON_IsString(dimensional_mode));
        TEST_ASSERT(cJSON_IsString(projection_policy));
        TEST_ASSERT(cJSON_IsString(extrusion_policy));
        TEST_ASSERT(strcmp(object_mode_intent->valuestring, "3d") == 0);
        TEST_ASSERT(strcmp(dimensional_mode->valuestring, "full_3d") == 0);
        TEST_ASSERT(strcmp(projection_policy->valuestring, "none") == 0);
        TEST_ASSERT(strcmp(extrusion_policy->valuestring, "none") == 0);
    }
    TEST_ASSERT(cJSON_GetObjectItem(obj, "locked_plane") == NULL);

    cJSON* transform = cJSON_GetObjectItem(obj, "transform");
    cJSON* position = cJSON_GetObjectItem(transform, "position");
    TEST_ASSERT(fabs(cJSON_GetObjectItem(position, "z")->valuedouble) > 0.0001);

    cJSON_Delete(root);
    shutdown_runtime();
    return true;
}

static bool test_canonical_scene_export_preserves_existing_extensions(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;
    const char* path = "/tmp/line_drawing_scene_export_preserve.json";

    const char* seed_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_seed\","
        "\"space_mode_default\":\"2d\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"objects\":[{"
            "\"object_id\":\"obj_line_drawing_layout\","
            "\"object_type\":\"curve_path\","
            "\"dimensional_mode\":\"plane_locked\","
            "\"transform\":{\"position\":{\"x\":0,\"y\":0,\"z\":0},\"rotation\":{\"x\":0,\"y\":0,\"z\":0},\"scale\":{\"x\":1,\"y\":1,\"z\":1}},"
            "\"geometry_ref\":{\"kind\":\"shape_asset\",\"id\":\"shape_line_drawing_layout\"},"
            "\"tags\":[\"authoring\"],"
            "\"flags\":{\"visible\":true,\"locked\":false,\"selectable\":true},"
            "\"extensions\":{\"ray_tracing\":{\"material\":\"glass\"}}"
        "}],"
        "\"constraints\":[],"
        "\"extensions\":{\"physics_sim\":{\"gravity\":9.8},\"custom_ns\":{\"keep\":true}}"
        "}";

    {
        FILE* f = fopen(path, "wb");
        TEST_ASSERT(f != NULL);
        TEST_ASSERT(fwrite(seed_json, 1u, strlen(seed_json), f) == strlen(seed_json));
        TEST_ASSERT(fclose(f) == 0);
    }

    Layout_AddWall(layout, (Vec2){ 0.0f, 0.0f }, (Vec2){ 1.0f, 0.0f });
    TEST_ASSERT(LineDrawingCanonicalScene_ExportLayoutToFile(layout, "scene_preserve", path));

    {
        FILE* f = fopen(path, "rb");
        long len = 0;
        char* buf = NULL;
        cJSON* root = NULL;
        cJSON* exts = NULL;
        cJSON* objects = NULL;
        cJSON* obj = NULL;
        cJSON* obj_ext = NULL;

        TEST_ASSERT(f != NULL);
        TEST_ASSERT(fseek(f, 0, SEEK_END) == 0);
        len = ftell(f);
        TEST_ASSERT(len > 0);
        TEST_ASSERT(fseek(f, 0, SEEK_SET) == 0);
        buf = (char*)malloc((size_t)len + 1u);
        TEST_ASSERT(buf != NULL);
        TEST_ASSERT(fread(buf, 1u, (size_t)len, f) == (size_t)len);
        buf[len] = '\0';
        TEST_ASSERT(fclose(f) == 0);

        root = cJSON_Parse(buf);
        free(buf);
        TEST_ASSERT(root != NULL);

        exts = cJSON_GetObjectItem(root, "extensions");
        TEST_ASSERT(cJSON_IsObject(exts));
        TEST_ASSERT(cJSON_GetObjectItem(exts, "physics_sim") != NULL);
        TEST_ASSERT(cJSON_GetObjectItem(exts, "custom_ns") != NULL);
        TEST_ASSERT(cJSON_GetObjectItem(exts, "line_drawing") != NULL);

        objects = cJSON_GetObjectItem(root, "objects");
        TEST_ASSERT(cJSON_IsArray(objects));
        TEST_ASSERT(cJSON_GetArraySize(objects) == 3);
        obj = find_object_by_id(objects, "obj_line_drawing_layout");
        TEST_ASSERT(cJSON_IsObject(obj));
        obj_ext = cJSON_GetObjectItem(obj, "extensions");
        TEST_ASSERT(cJSON_IsObject(obj_ext));
        TEST_ASSERT(cJSON_GetObjectItem(obj_ext, "ray_tracing") != NULL);
        TEST_ASSERT(cJSON_GetObjectItem(obj_ext, "line_drawing") != NULL);

        cJSON_Delete(root);
    }

    remove(path);
    shutdown_runtime();
    return true;
}

static bool test_canonical_scene_export_preserves_existing_scene_id_and_canonical_object_ids(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;
    const char* path = "/tmp/line_drawing_scene_export_id_immutability.json";

    const char* seed_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_authoring_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_existing_immutable\","
        "\"space_mode_intent\":\"2d\","
        "\"space_mode_default\":\"2d\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"objects\":["
            "{"
                "\"object_id\":\"obj_mutated_layout\","
                "\"object_type\":\"curve_path\","
                "\"space_mode_intent\":\"2d\","
                "\"dimensional_mode\":\"plane_locked\""
            "},"
            "{"
                "\"object_id\":\"obj_mutated_anchor_set\","
                "\"object_type\":\"point_set\","
                "\"space_mode_intent\":\"2d\","
                "\"dimensional_mode\":\"plane_locked\""
            "},"
            "{"
                "\"object_id\":\"obj_mutated_wall_set\","
                "\"object_type\":\"edge_set\","
                "\"space_mode_intent\":\"2d\","
                "\"dimensional_mode\":\"plane_locked\""
            "}"
        "],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";

    {
        FILE* f = fopen(path, "wb");
        TEST_ASSERT(f != NULL);
        TEST_ASSERT(fwrite(seed_json, 1u, strlen(seed_json), f) == strlen(seed_json));
        TEST_ASSERT(fclose(f) == 0);
    }

    Layout_AddWall(layout, (Vec2){ 0.0f, 0.0f }, (Vec2){ 4.0f, 0.0f });
    TEST_ASSERT(LineDrawingCanonicalScene_ExportLayoutToFile(layout, "scene_attempted_mutation", path));

    {
        FILE* f = fopen(path, "rb");
        long len = 0;
        char* buf = NULL;
        cJSON* root = NULL;
        cJSON* scene_id = NULL;
        cJSON* objects = NULL;

        TEST_ASSERT(f != NULL);
        TEST_ASSERT(fseek(f, 0, SEEK_END) == 0);
        len = ftell(f);
        TEST_ASSERT(len > 0);
        TEST_ASSERT(fseek(f, 0, SEEK_SET) == 0);
        buf = (char*)malloc((size_t)len + 1u);
        TEST_ASSERT(buf != NULL);
        TEST_ASSERT(fread(buf, 1u, (size_t)len, f) == (size_t)len);
        buf[len] = '\0';
        TEST_ASSERT(fclose(f) == 0);

        root = cJSON_Parse(buf);
        free(buf);
        TEST_ASSERT(root != NULL);

        scene_id = cJSON_GetObjectItem(root, "scene_id");
        TEST_ASSERT(cJSON_IsString(scene_id));
        TEST_ASSERT(strcmp(scene_id->valuestring, "scene_existing_immutable") == 0);

        objects = cJSON_GetObjectItem(root, "objects");
        TEST_ASSERT(cJSON_IsArray(objects));
        TEST_ASSERT(cJSON_GetArraySize(objects) == 3);
        TEST_ASSERT(find_object_by_id(objects, "obj_line_drawing_layout") != NULL);
        TEST_ASSERT(find_object_by_id(objects, "obj_line_drawing_anchor_set") != NULL);
        TEST_ASSERT(find_object_by_id(objects, "obj_line_drawing_wall_set") != NULL);
        TEST_ASSERT(find_object_by_id(objects, "obj_mutated_layout") == NULL);
        TEST_ASSERT(find_object_by_id(objects, "obj_mutated_anchor_set") == NULL);
        TEST_ASSERT(find_object_by_id(objects, "obj_mutated_wall_set") == NULL);

        cJSON_Delete(root);
    }

    remove(path);
    shutdown_runtime();
    return true;
}

static bool test_canonical_scene_export_includes_scene3d_and_primitive_payloads(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;
    PlanePrimitiveCreateParams plane_params;
    RectPrismPrimitiveCreateParams prism_params;
    uint32_t plane_id = 0u;
    uint32_t prism_id = 0u;
    char plane_object_id[64];
    char prism_object_id[64];

    layout->scene3d.bounds.enabled = true;
    layout->scene3d.bounds.clampOnEdit = true;
    layout->scene3d.bounds.min = (Vec3){ -6.0f, -5.0f, -4.0f };
    layout->scene3d.bounds.max = (Vec3){ 6.0f, 5.0f, 4.0f };
    Layout_ConstructionPlane3D_SetFromViewPlane(&layout->scene3d.constructionPlane,
                                                (ViewPlane){ .axis = VIEW_PLANE_YZ, .offset = 0.0f });

    Layout_PlanePrimitiveCreateParams_SetDefaults(&plane_params);
    plane_params.width = 4.0f;
    plane_params.height = 2.0f;
    TEST_ASSERT(Layout_CreatePlanePrimitive(layout, &plane_params, &plane_id, NULL));

    Layout_RectPrismPrimitiveCreateParams_SetDefaults(&prism_params);
    prism_params.width = 3.0f;
    prism_params.height = 2.0f;
    prism_params.depth = 1.5f;
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(layout, &prism_params, &prism_id, NULL));

    snprintf(plane_object_id, sizeof(plane_object_id), "obj3d_%u", plane_id);
    snprintf(prism_object_id, sizeof(prism_object_id), "obj3d_%u", prism_id);

    char* scene_json = LineDrawingCanonicalScene_ExportLayoutToString(layout, "scene_primitives");
    TEST_ASSERT(scene_json != NULL);

    cJSON* root = cJSON_Parse(scene_json);
    TEST_ASSERT(root != NULL);
    free(scene_json);

    {
        cJSON* space_mode_intent = cJSON_GetObjectItem(root, "space_mode_intent");
        cJSON* space_mode_default = cJSON_GetObjectItem(root, "space_mode_default");
        cJSON* hierarchy = cJSON_GetObjectItem(root, "hierarchy");
        cJSON* objects = cJSON_GetObjectItem(root, "objects");
        cJSON* extensions = cJSON_GetObjectItem(root, "extensions");
        cJSON* line_drawing_ext = cJSON_IsObject(extensions)
            ? cJSON_GetObjectItem(extensions, "line_drawing")
            : NULL;
        cJSON* scene3d = cJSON_IsObject(line_drawing_ext)
            ? cJSON_GetObjectItem(line_drawing_ext, "scene3d")
            : NULL;
        cJSON* bounds = cJSON_IsObject(scene3d)
            ? cJSON_GetObjectItem(scene3d, "bounds")
            : NULL;
        cJSON* construction_plane = cJSON_IsObject(scene3d)
            ? cJSON_GetObjectItem(scene3d, "construction_plane")
            : NULL;
        cJSON* plane_obj = NULL;
        cJSON* prism_obj = NULL;

        TEST_ASSERT(cJSON_IsString(space_mode_intent));
        TEST_ASSERT(cJSON_IsString(space_mode_default));
        TEST_ASSERT(strcmp(space_mode_intent->valuestring, "3d") == 0);
        TEST_ASSERT(strcmp(space_mode_default->valuestring, "3d") == 0);

        TEST_ASSERT(cJSON_IsArray(hierarchy));
        TEST_ASSERT(cJSON_GetArraySize(hierarchy) == 4);
        TEST_ASSERT(cJSON_IsArray(objects));
        TEST_ASSERT(cJSON_GetArraySize(objects) == 5);

        TEST_ASSERT(cJSON_IsObject(line_drawing_ext));
        TEST_ASSERT(cJSON_IsNumber(cJSON_GetObjectItem(line_drawing_ext, "active_object3d_count")));
        TEST_ASSERT(cJSON_GetObjectItem(line_drawing_ext, "active_object3d_count")->valueint == 2);

        TEST_ASSERT(cJSON_IsObject(bounds));
        TEST_ASSERT(cJSON_IsTrue(cJSON_GetObjectItem(bounds, "enabled")));
        TEST_ASSERT(cJSON_IsTrue(cJSON_GetObjectItem(bounds, "clamp_on_edit")));
        TEST_ASSERT(nearly_equal((float)cJSON_GetObjectItem(cJSON_GetObjectItem(bounds, "min"), "x")->valuedouble,
                                 -6.0f));
        TEST_ASSERT(nearly_equal((float)cJSON_GetObjectItem(cJSON_GetObjectItem(bounds, "max"), "z")->valuedouble,
                                 4.0f));

        TEST_ASSERT(cJSON_IsObject(construction_plane));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(construction_plane, "mode")->valuestring, "axis_aligned") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(construction_plane, "axis")->valuestring, "yz") == 0);
        TEST_ASSERT(nearly_equal((float)cJSON_GetObjectItem(construction_plane, "offset")->valuedouble, 0.0f));

        plane_obj = find_object_by_id(objects, plane_object_id);
        prism_obj = find_object_by_id(objects, prism_object_id);
        TEST_ASSERT(cJSON_IsObject(plane_obj));
        TEST_ASSERT(cJSON_IsObject(prism_obj));

        TEST_ASSERT(strcmp(cJSON_GetObjectItem(plane_obj, "object_type")->valuestring, "plane_primitive") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(plane_obj, "dimensional_mode")->valuestring, "plane_locked") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(plane_obj, "locked_plane")->valuestring, "yz") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(plane_obj, "projection_policy")->valuestring, "plane_yz_lock") == 0);

        {
            cJSON* plane_canonical = cJSON_GetObjectItem(plane_obj, "primitive");
            cJSON* plane_ext = cJSON_GetObjectItem(cJSON_GetObjectItem(plane_obj, "extensions"), "line_drawing");
            cJSON* plane_payload = cJSON_IsObject(plane_ext)
                ? cJSON_GetObjectItem(plane_ext, "primitive_payload")
                : NULL;
            TEST_ASSERT(cJSON_IsObject(plane_canonical));
            TEST_ASSERT(strcmp(cJSON_GetObjectItem(plane_canonical, "kind")->valuestring,
                               "plane_primitive") == 0);
            TEST_ASSERT(nearly_equal((float)cJSON_GetObjectItem(plane_canonical, "width")->valuedouble, 4.0f));
            TEST_ASSERT(nearly_equal((float)cJSON_GetObjectItem(plane_canonical, "height")->valuedouble, 2.0f));
            TEST_ASSERT(cJSON_IsObject(cJSON_GetObjectItem(plane_canonical, "frame")));
            TEST_ASSERT(cJSON_IsObject(cJSON_GetObjectItem(cJSON_GetObjectItem(plane_canonical, "frame"),
                                                           "axis_u")));
            TEST_ASSERT(cJSON_IsObject(plane_payload));
            TEST_ASSERT(strcmp(cJSON_GetObjectItem(plane_ext, "primitive_kind")->valuestring, "plane") == 0);
            TEST_ASSERT(nearly_equal((float)cJSON_GetObjectItem(plane_payload, "width")->valuedouble, 4.0f));
            TEST_ASSERT(nearly_equal((float)cJSON_GetObjectItem(plane_payload, "height")->valuedouble, 2.0f));
            TEST_ASSERT(cJSON_IsObject(cJSON_GetObjectItem(plane_payload, "frame")));
        }

        TEST_ASSERT(strcmp(cJSON_GetObjectItem(prism_obj, "object_type")->valuestring,
                           "rect_prism_primitive") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(prism_obj, "dimensional_mode")->valuestring, "full_3d") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(prism_obj, "space_mode_intent")->valuestring, "3d") == 0);

        {
            cJSON* prism_canonical = cJSON_GetObjectItem(prism_obj, "primitive");
            cJSON* prism_ext = cJSON_GetObjectItem(cJSON_GetObjectItem(prism_obj, "extensions"), "line_drawing");
            cJSON* prism_payload = cJSON_IsObject(prism_ext)
                ? cJSON_GetObjectItem(prism_ext, "primitive_payload")
                : NULL;
            TEST_ASSERT(cJSON_IsObject(prism_canonical));
            TEST_ASSERT(strcmp(cJSON_GetObjectItem(prism_canonical, "kind")->valuestring,
                               "rect_prism_primitive") == 0);
            TEST_ASSERT(nearly_equal((float)cJSON_GetObjectItem(prism_canonical, "depth")->valuedouble, 1.5f));
            TEST_ASSERT(cJSON_IsObject(cJSON_GetObjectItem(prism_canonical, "frame")));
            TEST_ASSERT(cJSON_IsObject(cJSON_GetObjectItem(cJSON_GetObjectItem(prism_canonical, "frame"),
                                                           "axis_u")));
            TEST_ASSERT(cJSON_IsObject(prism_payload));
            TEST_ASSERT(strcmp(cJSON_GetObjectItem(prism_ext, "primitive_kind")->valuestring, "rect_prism") == 0);
            TEST_ASSERT(nearly_equal((float)cJSON_GetObjectItem(prism_payload, "depth")->valuedouble, 1.5f));
            TEST_ASSERT(cJSON_IsObject(cJSON_GetObjectItem(prism_payload, "frame")));
        }
    }

    cJSON_Delete(root);
    shutdown_runtime();
    return true;
}

static bool test_layout_fixture_pure_3d_primitives_export_contract(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;
    char* fixture_json = read_text_file("tests/fixtures/ld3d3_primitive_layout_fixture.json");
    TEST_ASSERT(fixture_json != NULL);
    TEST_ASSERT(Layout_LoadFromString(layout, fixture_json));
    free(fixture_json);

    TEST_ASSERT(Layout_ObjectStore_LiveCount(&layout->objectStore) == 2u);
    TEST_ASSERT(layout->scene3d.bounds.enabled == true);
    TEST_ASSERT(layout->scene3d.constructionPlane.axisAligned.axis == VIEW_PLANE_YZ);

    {
        const Object3D* plane = Layout_ObjectStore_FindConst(&layout->objectStore, 1u);
        const Object3D* prism = Layout_ObjectStore_FindConst(&layout->objectStore, 2u);
        char* scene_json = NULL;
        cJSON* root = NULL;
        cJSON* objects = NULL;
        cJSON* plane_obj = NULL;
        cJSON* prism_obj = NULL;

        TEST_ASSERT(plane != NULL);
        TEST_ASSERT(prism != NULL);
        TEST_ASSERT(plane->kind == OBJECT3D_KIND_PLANE);
        TEST_ASSERT(prism->kind == OBJECT3D_KIND_RECT_PRISM);
        TEST_ASSERT(nearly_equal(plane->plane.width, 4.5f));
        TEST_ASSERT(nearly_equal(prism->rectPrism.depth, 1.5f));

        scene_json = LineDrawingCanonicalScene_ExportLayoutToString(layout, "scene_fixture_pure_3d");
        TEST_ASSERT(scene_json != NULL);
        root = cJSON_Parse(scene_json);
        TEST_ASSERT(root != NULL);
        free(scene_json);

        TEST_ASSERT(strcmp(cJSON_GetObjectItem(root, "space_mode_default")->valuestring, "3d") == 0);
        objects = cJSON_GetObjectItem(root, "objects");
        TEST_ASSERT(cJSON_IsArray(objects));
        plane_obj = find_object_by_id(objects, "obj3d_1");
        prism_obj = find_object_by_id(objects, "obj3d_2");
        TEST_ASSERT(cJSON_IsObject(plane_obj));
        TEST_ASSERT(cJSON_IsObject(prism_obj));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(cJSON_GetObjectItem(plane_obj, "primitive"), "kind")->valuestring,
                           "plane_primitive") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(cJSON_GetObjectItem(prism_obj, "primitive"), "kind")->valuestring,
                           "rect_prism_primitive") == 0);
        cJSON_Delete(root);
    }

    shutdown_runtime();
    return true;
}

static bool test_layout_fixture_mixed_2d_3d_export_contract(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;
    char* fixture_json = read_text_file("tests/fixtures/ld3d4_mixed_layout_fixture.json");
    TEST_ASSERT(fixture_json != NULL);
    TEST_ASSERT(Layout_LoadFromString(layout, fixture_json));
    free(fixture_json);

    TEST_ASSERT(layout->anchorCount == 4u);
    TEST_ASSERT(layout->wallCount == 4u);
    TEST_ASSERT(Layout_ObjectStore_LiveCount(&layout->objectStore) == 1u);

    {
        char* scene_json = LineDrawingCanonicalScene_ExportLayoutToString(layout, "scene_fixture_mixed");
        cJSON* root = NULL;
        cJSON* objects = NULL;
        cJSON* layout_obj = NULL;
        cJSON* prism_obj = NULL;
        TEST_ASSERT(scene_json != NULL);
        root = cJSON_Parse(scene_json);
        TEST_ASSERT(root != NULL);
        free(scene_json);

        TEST_ASSERT(strcmp(cJSON_GetObjectItem(root, "space_mode_default")->valuestring, "3d") == 0);
        objects = cJSON_GetObjectItem(root, "objects");
        TEST_ASSERT(cJSON_IsArray(objects));
        layout_obj = find_object_by_id(objects, "obj_line_drawing_layout");
        prism_obj = find_object_by_id(objects, "obj3d_7");
        TEST_ASSERT(cJSON_IsObject(layout_obj));
        TEST_ASSERT(cJSON_IsObject(prism_obj));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(layout_obj, "object_type")->valuestring, "curve_path") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(cJSON_GetObjectItem(prism_obj, "primitive"), "kind")->valuestring,
                           "rect_prism_primitive") == 0);
        cJSON_Delete(root);
    }

    shutdown_runtime();
    return true;
}

static bool test_canonical_scene_export_applies_scene_authoring_options(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;
    LineDrawingSceneAuthoringOptions options = {
        .material_id = "mat_custom",
        .material_type = "flat_color",
        .light_id = "light_custom",
        .light_type = "point",
        .camera_id = "cam_custom",
        .camera_type = "perspective",
    };

    Layout_AddWall3(layout, (Vec3){ 0.0f, 0.0f, 0.0f }, (Vec3){ 0.0f, 2.0f, 2.0f });

    char* scene_json =
        LineDrawingCanonicalScene_ExportLayoutToStringWithOptions(layout, "scene_options", &options);
    TEST_ASSERT(scene_json != NULL);

    cJSON* root = cJSON_Parse(scene_json);
    TEST_ASSERT(root != NULL);
    free(scene_json);

    {
        cJSON* materials = cJSON_GetObjectItem(root, "materials");
        cJSON* lights = cJSON_GetObjectItem(root, "lights");
        cJSON* cameras = cJSON_GetObjectItem(root, "cameras");
        cJSON* objects = cJSON_GetObjectItem(root, "objects");
        TEST_ASSERT(cJSON_IsArray(materials));
        TEST_ASSERT(cJSON_IsArray(lights));
        TEST_ASSERT(cJSON_IsArray(cameras));
        TEST_ASSERT(cJSON_IsArray(objects));
        TEST_ASSERT(cJSON_GetArraySize(materials) == 1);
        TEST_ASSERT(cJSON_GetArraySize(lights) == 1);
        TEST_ASSERT(cJSON_GetArraySize(cameras) == 1);

        cJSON* material = cJSON_GetArrayItem(materials, 0);
        cJSON* light = cJSON_GetArrayItem(lights, 0);
        cJSON* camera = cJSON_GetArrayItem(cameras, 0);
        TEST_ASSERT(cJSON_IsObject(material));
        TEST_ASSERT(cJSON_IsObject(light));
        TEST_ASSERT(cJSON_IsObject(camera));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(material, "material_id")->valuestring, "mat_custom") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(material, "material_type")->valuestring, "flat_color") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(light, "light_id")->valuestring, "light_custom") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(light, "light_type")->valuestring, "point") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(camera, "camera_id")->valuestring, "cam_custom") == 0);
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(camera, "camera_type")->valuestring, "perspective") == 0);

        cJSON* obj = find_object_by_id(objects, "obj_line_drawing_layout");
        TEST_ASSERT(cJSON_IsObject(obj));
        cJSON* mat_ref = cJSON_GetObjectItem(obj, "material_ref");
        TEST_ASSERT(cJSON_IsObject(mat_ref));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(mat_ref, "id")->valuestring, "mat_custom") == 0);
    }

    cJSON_Delete(root);
    shutdown_runtime();
    return true;
}

static bool test_canonical_scene_export_rejects_invalid_scene_authoring_options(void) {
    init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;
    const char* path = "/tmp/line_drawing_scene_export_invalid_options.json";
    LineDrawingSceneAuthoringOptions bad_options = {
        .material_id = "bad id",
        .material_type = "flat_color",
        .light_id = "light_custom",
        .light_type = "point",
        .camera_id = "cam_custom",
        .camera_type = "perspective",
    };

    Layout_AddWall(layout, (Vec2){ 0.0f, 0.0f }, (Vec2){ 1.0f, 1.0f });
    TEST_ASSERT(LineDrawingCanonicalScene_ExportLayoutToStringWithOptions(
                    layout, "scene_bad_options", &bad_options) == NULL);
    TEST_ASSERT(!LineDrawingCanonicalScene_ExportLayoutToFileWithOptions(
        layout, "scene_bad_options", path, &bad_options));
    remove(path);
    shutdown_runtime();
    return true;
}

bool layout_run_tests(void) {
    const TestCase cases[] = {
        { "AddWallReusesAnchors", test_layout_add_wall_reuses_anchors },
        { "RemoveWallCompactsOnDemand", test_layout_remove_wall_compacts_on_demand },
        { "LayoutStringRoundtrip", test_layout_string_roundtrip },
        { "EditorUndoRedoRestoresLayout", test_editor_undo_redo_restores_layout },
        { "LayoutJsonEmbedsVersion", test_layout_json_embeds_version },
        { "LayoutJsonFutureVersionRejected", test_layout_json_future_version_rejected },
        { "LayoutJsonMissingVersionDefaults", test_layout_json_missing_version_defaults },
        { "LayoutSceneBoundsClampContract", test_layout_scene_bounds_clamp_contract },
        { "LayoutJsonV6PersistsSceneBounds", test_layout_json_v6_persists_scene_bounds },
        { "LayoutJsonMissingScene3DDefaultsSceneBounds", test_layout_json_missing_scene3d_defaults_scene_bounds },
        { "ConstructionPlaneAxisModeMapsToViewContext", test_construction_plane_axis_mode_maps_to_view_context },
        { "ConstructionPlaneCustomFrameValidationAndProjectionFallback", test_construction_plane_custom_frame_validation_and_projection_fallback },
        { "LayoutJsonV6PersistsConstructionPlaneCustomFrame", test_layout_json_v6_persists_construction_plane_custom_frame },
        { "ObjectStoreIdStabilityAndTombstoneDelete", test_object_store_id_stability_and_tombstone_delete },
        { "ObjectStorePlaneLockDimensionalRulesEnforced", test_object_store_plane_lock_dimensional_rules_enforced },
        { "ObjectStoreRejectsInvalidTransformScale", test_object_store_rejects_invalid_transform_scale },
        { "ObjectStoreRectPrismPayloadValidation", test_object_store_rect_prism_payload_validation },
        { "PlanePrimitiveCreationRespectsBoundsAndConstructionPlane", test_plane_primitive_creation_respects_bounds_and_construction_plane },
        { "RectPrismPrimitiveCreationRespectsBoundsAndConstructionPlane", test_rect_prism_primitive_creation_respects_bounds_and_construction_plane },
        { "RectPrismPrimitiveCreationClampsDepthToZeroAtBoundsLimit",
          test_rect_prism_primitive_creation_clamps_depth_to_zero_at_bounds_limit },
        { "LayoutObject3DComputeRectPrismCornersContract", test_layout_object3d_compute_rect_prism_corners_contract },
        { "LayoutJsonV8PersistsPlanePrimitivesDeterministically", test_layout_json_v8_persists_plane_primitives_deterministically },
        { "LayoutJsonV8PersistsRectPrismPayloadDeterministically", test_layout_json_v8_persists_rect_prism_payload_deterministically },
        { "PlaneResizeCornerDragUpdatesSizeAndCenter", test_plane_resize_corner_drag_updates_size_and_center },
        { "PlaneResizeEdgeDragKeepsOppositeEdgeFixed", test_plane_resize_edge_drag_keeps_opposite_edge_fixed },
        { "PlaneResizeEdgeDragClampsToMinSize", test_plane_resize_edge_drag_clamps_to_min_size },
        { "PlaneResizeCornerDragCrossesAnchorAndFlipsAxes", test_plane_resize_corner_drag_crosses_anchor_and_flips_axes },
        { "PlaneResizeHandleResolutionFlipsCornerAxes", test_plane_resize_handle_resolution_flips_corner_axes },
        { "PlaneResizeHandleResolutionFlipsEdgeAxis", test_plane_resize_handle_resolution_flips_edge_axis },
        { "RectPrismResizeCornerDragUpdatesSizeAndCenter", test_rect_prism_resize_corner_drag_updates_size_and_center },
        { "RectPrismResizeHandleResolutionFlipsCornerAxes", test_rect_prism_resize_handle_resolution_flips_corner_axes },
        { "RectPrismDepthResizeFromFaceHandleUpdatesDepthAndCenter",
          test_rect_prism_depth_resize_from_face_handle_updates_depth_and_center },
        { "RectPrismSetDimensionsPreservesCenterAndUpdatesSize",
          test_rect_prism_set_dimensions_preserves_center_and_updates_size },
        { "RectPrismSetDimensionsClampsToBoundsWhenLocked",
          test_rect_prism_set_dimensions_clamps_to_bounds_when_locked },
        { "RectPrismSetDimensionsRejectsInvalidValues",
          test_rect_prism_set_dimensions_rejects_invalid_values },
        { "LayoutSetObject3DPositionMovesPlaneAndPreservesDimensions",
          test_layout_set_object3d_position_moves_plane_and_preserves_dimensions },
        { "LayoutSetObject3DPositionClampsLockedPrismToBounds",
          test_layout_set_object3d_position_clamps_locked_prism_to_bounds },
        { "LayoutRotateObject3DRotatesPlaneFrameAndPreservesDimensions",
          test_layout_rotate_object3d_rotates_plane_frame_and_preserves_dimensions },
        { "LayoutRotateObject3DBaselineCompositionIsDeterministic",
          test_layout_rotate_object3d_baseline_composition_is_deterministic },
        { "LayoutRotateObject3DClampsLockedBounds",
          test_layout_rotate_object3d_clamps_locked_bounds },
        { "UIPanelDisplayUnitConversionRoundtrip",
          test_ui_panel_display_unit_conversion_roundtrip },
        { "UIPanelDisplayUnitRoundingDeterminism",
          test_ui_panel_display_unit_rounding_determinism },
        { "LayoutJsonPreservesAnchorHandles", test_layout_json_preserves_anchor_handles },
        { "LayoutJsonV3PersistsAnchorZ", test_layout_json_v3_persists_anchor_z },
        { "LayoutJsonV2DefaultsZToZero", test_layout_json_v2_defaults_z_to_zero },
        { "LayoutJsonV4DefaultsZOmitted", test_layout_json_v4_defaults_z_to_zero_when_omitted },
        { "LayoutJsonAcceptsAdditiveUnknownFields", test_layout_json_accepts_additive_unknown_fields },
        { "LayoutHandlesLinkToggle", test_layout_handles_link_toggle },
        { "EditorHistoryLimitEnforced", test_editor_history_limit_enforced },
        { "LayoutAddAnchor3PreservesZ", test_layout_add_anchor3_preserves_z },
        { "LayoutCornerAnchorAllowsMoreThanTwoConnections", test_layout_corner_anchor_allows_more_than_two_connections },
        { "HitboxPrefersNearerPlaneDepthForOverlappingPoints", test_hitbox_prefers_nearer_plane_depth_for_overlapping_points },
        { "HitboxGizmoAxisEmitsForSelectedAnchorInFreeView", test_hitbox_gizmo_axis_emits_for_selected_anchor_in_free_view },
        { "HitboxGizmoAxisDisabledWhenFreeViewOff", test_hitbox_gizmo_axis_disabled_when_free_view_off },
        { "HitboxPlaneObjectIsSelectable", test_hitbox_plane_object_is_selectable },
        { "HitboxPlaneResizeHandlesEmitForSelectedPlane", test_hitbox_plane_resize_handles_emit_for_selected_plane },
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
        { "LayoutComputeCentroidIgnoresDeletedAnchors", test_layout_compute_centroid_ignores_deleted_anchors },
        { "ShapeExportProjectionAxisMapping", test_shape_export_projection_axis_mapping },
        { "SpaceModeToggleContract2DResetsPlaneAndFreeView",
          test_space_mode_toggle_contract_2d_resets_plane_and_free_view },
        { "CanonicalSceneExport2DPayload", test_canonical_scene_export_2d_payload },
        { "CanonicalSceneExport3DPayload", test_canonical_scene_export_3d_payload },
        { "CanonicalSceneExportPreservesExistingExtensions", test_canonical_scene_export_preserves_existing_extensions },
        { "CanonicalSceneExportPreservesExistingSceneIdAndCanonicalObjectIds",
          test_canonical_scene_export_preserves_existing_scene_id_and_canonical_object_ids },
        { "CanonicalSceneExportIncludesScene3DAndPrimitivePayloads",
          test_canonical_scene_export_includes_scene3d_and_primitive_payloads },
        { "LayoutFixturePure3DPrimitivesExportContract",
          test_layout_fixture_pure_3d_primitives_export_contract },
        { "LayoutFixtureMixed2D3DExportContract",
          test_layout_fixture_mixed_2d_3d_export_contract },
        { "CanonicalSceneExportAppliesSceneAuthoringOptions",
          test_canonical_scene_export_applies_scene_authoring_options },
        { "CanonicalSceneExportRejectsInvalidSceneAuthoringOptions",
          test_canonical_scene_export_rejects_invalid_scene_authoring_options }
    };

    return run_test_cases("Layout", cases, sizeof(cases) / sizeof(cases[0]));
}
