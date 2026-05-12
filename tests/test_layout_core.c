#include "test_layout_internal.h"

static bool test_layout_add_wall_reuses_anchors(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    Vec2 a = {0.0f, 0.0f};
    Vec2 b = {2.0f, 0.0f};

    Layout_AddWall(layout, a, b);
    TEST_ASSERT(layout->wallCount == 1);
    TEST_ASSERT(layout->anchorCount == 2);

    Layout_AddWall(layout, a, b);
    TEST_ASSERT(layout->wallCount == 2);
    TEST_ASSERT(layout->anchorCount == 2);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_remove_wall_compacts_on_demand(void) {
    ld_test_init_runtime();
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

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_string_roundtrip(void) {
    ld_test_init_runtime();
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

    ld_test_shutdown_runtime();
    return true;
}

static bool test_editor_undo_redo_restores_layout(void) {
    ld_test_init_runtime();
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

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_json_embeds_version(void) {
    ld_test_init_runtime();
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
    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_json_future_version_rejected(void) {
    ld_test_init_runtime();
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
    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_json_missing_version_defaults(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    const char* legacyJson = "{\"anchors\":[{\"x\":0,\"y\":0,\"persistent\":true}],\"walls\":[]}";
    TEST_ASSERT(Layout_LoadFromString(layout, legacyJson));
    TEST_ASSERT(layout->anchorCount == 1);
    TEST_ASSERT(layout->anchors[0].isPersistent == true);
    TEST_ASSERT(layout->gridSize == 1.0f);
    TEST_ASSERT(ld_test_nearly_equal(layout->anchors[0].pos.z, 0.0f));
    TEST_ASSERT(layout->anchors[0].handleAxis == VIEW_PLANE_XY);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_scene_bounds_clamp_contract(void) {
    ld_test_init_runtime();
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
    TEST_ASSERT(ld_test_nearly_equal(point.x, 1.0f));
    TEST_ASSERT(ld_test_nearly_equal(point.y, -2.0f));
    TEST_ASSERT(ld_test_nearly_equal(point.z, 0.5f));

    bounds.enabled = false;
    point = (Vec3){ 99.0f, -99.0f, 50.0f };
    clamped = true;
    TEST_ASSERT(Layout_SceneBounds3D_ClampPoint(&bounds, &point, &clamped));
    TEST_ASSERT(!clamped);
    TEST_ASSERT(ld_test_nearly_equal(point.x, 99.0f));
    TEST_ASSERT(ld_test_nearly_equal(point.y, -99.0f));
    TEST_ASSERT(ld_test_nearly_equal(point.z, 50.0f));

    bounds.enabled = true;
    bounds.min.x = 2.0f;
    bounds.max.x = 1.0f;
    TEST_ASSERT(!Layout_SceneBounds3D_IsValid(&bounds));
    TEST_ASSERT(!Layout_SceneBounds3D_ClampPoint(&bounds, &point, NULL));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_scene_bounds_face_handle_resize_contract(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;
    SceneBounds3D* bounds = &layout->scene3d.bounds;

    bounds->enabled = true;
    bounds->min = (Vec3){ -2.0f, -3.0f, -4.0f };
    bounds->max = (Vec3){  2.0f,  3.0f,  4.0f };

    RectPrismHandleAxisMask mask = {0};
    TEST_ASSERT(Layout_SceneBoundsHandleAxisMask(SCENE_BOUNDS_HANDLE_MAX_X, &mask));
    TEST_ASSERT(mask.allowU);
    TEST_ASSERT(!mask.allowV);
    TEST_ASSERT(!mask.allowN);

    Vec3 handleWorld = {0};
    TEST_ASSERT(Layout_SceneBoundsHandleWorldPoint(bounds,
                                                   SCENE_BOUNDS_HANDLE_MAX_X,
                                                   &handleWorld));
    TEST_ASSERT(ld_test_vec3_nearly_equal(handleWorld, (Vec3){ 2.0f, 0.0f, 0.0f }));

    TEST_ASSERT(Layout_ResizeSceneBounds3DFromHandle(layout,
                                                     SCENE_BOUNDS_HANDLE_MAX_X,
                                                     (Vec3){ 6.0f, 99.0f, 99.0f }));
    TEST_ASSERT(ld_test_nearly_equal(bounds->max.x, 6.0f));
    TEST_ASSERT(ld_test_nearly_equal(bounds->max.y, 3.0f));
    TEST_ASSERT(ld_test_nearly_equal(bounds->max.z, 4.0f));
    TEST_ASSERT(Layout_SceneBounds3D_IsValid(bounds));

    TEST_ASSERT(Layout_ResizeSceneBounds3DFromHandle(layout,
                                                     SCENE_BOUNDS_HANDLE_MIN_Y,
                                                     (Vec3){ 99.0f, 10.0f, 99.0f }));
    TEST_ASSERT(ld_test_nearly_equal(bounds->min.y, bounds->max.y));
    TEST_ASSERT(Layout_SceneBounds3D_IsValid(bounds));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_scene_bounds_edge_and_corner_handle_resize_contract(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;
    SceneBounds3D* bounds = &layout->scene3d.bounds;

    bounds->enabled = true;
    bounds->min = (Vec3){ -2.0f, -3.0f, -4.0f };
    bounds->max = (Vec3){  2.0f,  3.0f,  4.0f };

    RectPrismHandleAxisMask edgeMask = {0};
    TEST_ASSERT(Layout_SceneBoundsHandleAxisMask(SCENE_BOUNDS_HANDLE_EDGE_X_MIN_Y_MAX_Z,
                                                 &edgeMask));
    TEST_ASSERT(!edgeMask.allowU);
    TEST_ASSERT(edgeMask.allowV);
    TEST_ASSERT(edgeMask.allowN);

    Vec3 edgeWorld = {0};
    TEST_ASSERT(Layout_SceneBoundsHandleWorldPoint(bounds,
                                                   SCENE_BOUNDS_HANDLE_EDGE_X_MIN_Y_MAX_Z,
                                                   &edgeWorld));
    TEST_ASSERT(ld_test_vec3_nearly_equal(edgeWorld, (Vec3){ 0.0f, -3.0f, 4.0f }));

    TEST_ASSERT(Layout_ResizeSceneBounds3DFromHandle(layout,
                                                     SCENE_BOUNDS_HANDLE_EDGE_X_MIN_Y_MAX_Z,
                                                     (Vec3){ 99.0f, -1.0f, 7.0f }));
    TEST_ASSERT(ld_test_nearly_equal(bounds->min.x, -2.0f));
    TEST_ASSERT(ld_test_nearly_equal(bounds->max.x, 2.0f));
    TEST_ASSERT(ld_test_nearly_equal(bounds->min.y, -1.0f));
    TEST_ASSERT(ld_test_nearly_equal(bounds->max.z, 7.0f));

    RectPrismHandleAxisMask cornerMask = {0};
    TEST_ASSERT(Layout_SceneBoundsHandleAxisMask(SCENE_BOUNDS_HANDLE_CORNER_MAX_X_MIN_Y_MAX_Z,
                                                 &cornerMask));
    TEST_ASSERT(cornerMask.allowU);
    TEST_ASSERT(cornerMask.allowV);
    TEST_ASSERT(cornerMask.allowN);

    TEST_ASSERT(Layout_ResizeSceneBounds3DFromHandle(layout,
                                                     SCENE_BOUNDS_HANDLE_CORNER_MAX_X_MIN_Y_MAX_Z,
                                                     (Vec3){ 5.0f, -10.0f, 1.0f }));
    TEST_ASSERT(ld_test_nearly_equal(bounds->max.x, 5.0f));
    TEST_ASSERT(ld_test_nearly_equal(bounds->min.y, -10.0f));
    TEST_ASSERT(ld_test_nearly_equal(bounds->max.z, 1.0f));
    TEST_ASSERT(Layout_SceneBounds3D_IsValid(bounds));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_scene_bounds_center_handle_translate_contract(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;
    SceneBounds3D* bounds = &layout->scene3d.bounds;

    bounds->enabled = true;
    bounds->min = (Vec3){ -2.0f, -3.0f, -4.0f };
    bounds->max = (Vec3){  2.0f,  3.0f,  4.0f };

    RectPrismHandleAxisMask mask = {0};
    TEST_ASSERT(Layout_SceneBoundsHandleAxisMask(SCENE_BOUNDS_HANDLE_CENTER, &mask));
    TEST_ASSERT(mask.allowU);
    TEST_ASSERT(mask.allowV);
    TEST_ASSERT(mask.allowN);

    Vec3 centerWorld = {0};
    TEST_ASSERT(Layout_SceneBoundsHandleWorldPoint(bounds,
                                                   SCENE_BOUNDS_HANDLE_CENTER,
                                                   &centerWorld));
    TEST_ASSERT(ld_test_vec3_nearly_equal(centerWorld, (Vec3){ 0.0f, 0.0f, 0.0f }));
    TEST_ASSERT(!Layout_ResizeSceneBounds3DFromHandle(layout,
                                                      SCENE_BOUNDS_HANDLE_CENTER,
                                                      (Vec3){ 10.0f, 10.0f, 10.0f }));
    TEST_ASSERT(ld_test_vec3_nearly_equal(bounds->min, (Vec3){ -2.0f, -3.0f, -4.0f }));
    TEST_ASSERT(ld_test_vec3_nearly_equal(bounds->max, (Vec3){  2.0f,  3.0f,  4.0f }));

    TEST_ASSERT(Layout_TranslateSceneBounds3D(layout, (Vec3){ 5.0f, -1.0f, 2.0f }));
    TEST_ASSERT(ld_test_vec3_nearly_equal(bounds->min, (Vec3){ 3.0f, -4.0f, -2.0f }));
    TEST_ASSERT(ld_test_vec3_nearly_equal(bounds->max, (Vec3){ 7.0f,  2.0f,  6.0f }));
    TEST_ASSERT(Layout_SceneBounds3D_IsValid(bounds));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_fit_scene_bounds_to_object_uses_world_aabb(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    RectPrismPrimitiveCreateParams params;
    Layout_RectPrismPrimitiveCreateParams_SetDefaults(&params);
    params.width = 4.0f;
    params.height = 6.0f;
    params.depth = 8.0f;
    params.lockToBounds = false;
    params.useExplicitFrame = true;
    params.explicitFrame = (PlaneFrame3){
        .origin = { 10.0f, -5.0f, 2.0f },
        .axisU = { 1.0f, 0.0f, 0.0f },
        .axisV = { 0.0f, 1.0f, 0.0f },
        .normal = { 0.0f, 0.0f, 1.0f }
    };

    uint32_t objectId = 0u;
    TEST_ASSERT(Layout_CreateRectPrismPrimitive(layout, &params, &objectId, NULL));
    TEST_ASSERT(Layout_FitSceneBounds3DToObject(layout, objectId, 1.0f));
    TEST_ASSERT(layout->scene3d.bounds.enabled);
    TEST_ASSERT(ld_test_vec3_nearly_equal(layout->scene3d.bounds.min,
                                          (Vec3){ 7.0f, -9.0f, -3.0f }));
    TEST_ASSERT(ld_test_vec3_nearly_equal(layout->scene3d.bounds.max,
                                          (Vec3){ 13.0f, -1.0f, 7.0f }));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_json_v6_persists_scene_bounds(void) {
    ld_test_init_runtime();
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
    TEST_ASSERT(ld_test_nearly_equal((float)cJSON_GetObjectItem(min, "x")->valuedouble, -7.0f));
    TEST_ASSERT(ld_test_nearly_equal((float)cJSON_GetObjectItem(max, "z")->valuedouble, 9.0f));
    cJSON_Delete(root);

    TEST_ASSERT(Layout_LoadFromString(layout, json));
    free(json);
    TEST_ASSERT(layout->scene3d.bounds.enabled);
    TEST_ASSERT(layout->scene3d.bounds.clampOnEdit);
    TEST_ASSERT(ld_test_nearly_equal(layout->scene3d.bounds.min.x, -7.0f));
    TEST_ASSERT(ld_test_nearly_equal(layout->scene3d.bounds.min.y, -8.0f));
    TEST_ASSERT(ld_test_nearly_equal(layout->scene3d.bounds.min.z, -9.0f));
    TEST_ASSERT(ld_test_nearly_equal(layout->scene3d.bounds.max.x, 7.0f));
    TEST_ASSERT(ld_test_nearly_equal(layout->scene3d.bounds.max.y, 8.0f));
    TEST_ASSERT(ld_test_nearly_equal(layout->scene3d.bounds.max.z, 9.0f));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_json_missing_scene3d_defaults_scene_bounds(void) {
    ld_test_init_runtime();
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
    TEST_ASSERT(ld_test_nearly_equal(layout->scene3d.bounds.min.x, defaults.bounds.min.x));
    TEST_ASSERT(ld_test_nearly_equal(layout->scene3d.bounds.min.y, defaults.bounds.min.y));
    TEST_ASSERT(ld_test_nearly_equal(layout->scene3d.bounds.min.z, defaults.bounds.min.z));
    TEST_ASSERT(ld_test_nearly_equal(layout->scene3d.bounds.max.x, defaults.bounds.max.x));
    TEST_ASSERT(ld_test_nearly_equal(layout->scene3d.bounds.max.y, defaults.bounds.max.y));
    TEST_ASSERT(ld_test_nearly_equal(layout->scene3d.bounds.max.z, defaults.bounds.max.z));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_construction_plane_axis_mode_maps_to_view_context(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_3D, false));

    ViewPlane legacyPlane = { .axis = VIEW_PLANE_YZ, .offset = 4.25f };
    state->activePlane = (ViewPlane){ .axis = VIEW_PLANE_XY, .offset = 0.0f };
    Layout_ConstructionPlane3D_SetFromViewPlane(&state->layout.scene3d.constructionPlane, legacyPlane);

    SpaceViewContext viewCtx = SpaceAdapter_BuildViewContext(state);
    TEST_ASSERT(viewCtx.plane.axis == VIEW_PLANE_YZ);
    TEST_ASSERT(ld_test_nearly_equal(viewCtx.plane.offset, 4.25f));

    ld_test_shutdown_runtime();
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
        TEST_ASSERT(ld_test_nearly_equal(fallback.offset, 6.5f));
    }

    plane.customFrame.axisV = plane.customFrame.axisU;
    TEST_ASSERT(!Layout_ConstructionPlane3D_IsValid(&plane));

    return true;
}

static bool test_layout_json_v6_persists_construction_plane_custom_frame(void) {
    ld_test_init_runtime();
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
        TEST_ASSERT(ld_test_nearly_equal((float)cJSON_GetObjectItem(origin, "x")->valuedouble, 3.5f));
    }
    cJSON_Delete(root);

    TEST_ASSERT(Layout_LoadFromString(layout, json));
    free(json);
    TEST_ASSERT(layout->scene3d.constructionPlane.mode == CONSTRUCTION_PLANE_MODE_CUSTOM_FRAME);
    TEST_ASSERT(Layout_ConstructionPlane3D_IsValid(&layout->scene3d.constructionPlane));
    {
        ViewPlane fallback = Layout_ConstructionPlane3D_ToViewPlane(&layout->scene3d.constructionPlane);
        TEST_ASSERT(fallback.axis == VIEW_PLANE_YZ);
        TEST_ASSERT(ld_test_nearly_equal(fallback.offset, 3.5f));
    }

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_json_preserves_anchor_handles(void) {
    ld_test_init_runtime();
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
    TEST_ASSERT(ld_test_nearly_equal(loaded->handleInLength, 2.5f));
    TEST_ASSERT(ld_test_nearly_equal(loaded->handleInAngleDeg, 45.0f));
    TEST_ASSERT(ld_test_nearly_equal(loaded->handleOutLength, 1.25f));
    TEST_ASSERT(ld_test_nearly_equal(loaded->handleOutAngleDeg, -60.0f));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_json_v3_persists_anchor_z(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    int idx = Layout_AddAnchor3(layout, (Vec3){ 1.0f, 2.0f, 7.25f });
    TEST_ASSERT(idx >= 0);

    char* snapshot = Layout_SaveToString(layout);
    TEST_ASSERT(snapshot != NULL);
    TEST_ASSERT(Layout_LoadFromString(layout, snapshot));
    free(snapshot);

    TEST_ASSERT(layout->anchorCount == 1);
    TEST_ASSERT(ld_test_nearly_equal(layout->anchors[0].pos.x, 1.0f));
    TEST_ASSERT(ld_test_nearly_equal(layout->anchors[0].pos.y, 2.0f));
    TEST_ASSERT(ld_test_nearly_equal(layout->anchors[0].pos.z, 7.25f));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_json_v2_defaults_z_to_zero(void) {
    ld_test_init_runtime();
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
    TEST_ASSERT(ld_test_nearly_equal(layout->anchors[0].pos.x, 3.0f));
    TEST_ASSERT(ld_test_nearly_equal(layout->anchors[0].pos.y, 4.0f));
    TEST_ASSERT(ld_test_nearly_equal(layout->anchors[0].pos.z, 0.0f));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_json_v4_defaults_z_to_zero_when_omitted(void) {
    ld_test_init_runtime();
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
    TEST_ASSERT(ld_test_nearly_equal(layout->gridSize, 2.0f));
    TEST_ASSERT(ld_test_nearly_equal(layout->anchors[0].pos.x, 9.0f));
    TEST_ASSERT(ld_test_nearly_equal(layout->anchors[0].pos.y, -3.0f));
    TEST_ASSERT(ld_test_nearly_equal(layout->anchors[0].pos.z, 0.0f));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_json_accepts_additive_unknown_fields(void) {
    ld_test_init_runtime();
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
    TEST_ASSERT(ld_test_nearly_equal(layout->gridSize, 1.5f));
    TEST_ASSERT(ld_test_nearly_equal(layout->anchors[0].pos.z, 2.0f));
    TEST_ASSERT(layout->anchors[0].type == ANCHOR_TYPE_CURVE);
    TEST_ASSERT(layout->anchors[0].handleAxis == VIEW_PLANE_XZ);
    TEST_ASSERT(ld_test_nearly_equal(layout->anchors[0].handleInLength, 1.25f));
    TEST_ASSERT(ld_test_nearly_equal(layout->anchors[0].handleOutAngleDeg, -75.0f));
    TEST_ASSERT(layout->anchors[1].isPersistent == true);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_handles_link_toggle(void) {
    ld_test_init_runtime();
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
    TEST_ASSERT(ld_test_nearly_equal(anchor->handleInLength, 2.0f));
    TEST_ASSERT(ld_test_nearly_equal(anchor->handleOutLength, 2.0f));
    TEST_ASSERT(ld_test_nearly_equal(anchor->handleOutAngleDeg,
                                     Angle_NormalizeDeg(anchor->handleInAngleDeg + 180.0f)));

    TEST_ASSERT(Layout_SetHandlesLinked(layout, idx, false));
    TEST_ASSERT(anchor->handlesLinked == false);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_editor_history_limit_enforced(void) {
    ld_test_init_runtime();
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

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_add_anchor3_preserves_z(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    int idx = Layout_AddAnchor3(layout, (Vec3){ 1.0f, 2.0f, 3.5f });
    TEST_ASSERT(idx >= 0);
    TEST_ASSERT(layout->anchorCount == 1);
    TEST_ASSERT(ld_test_nearly_equal(layout->anchors[0].pos.x, 1.0f));
    TEST_ASSERT(ld_test_nearly_equal(layout->anchors[0].pos.y, 2.0f));
    TEST_ASSERT(ld_test_nearly_equal(layout->anchors[0].pos.z, 3.5f));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_corner_anchor_allows_more_than_two_connections(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    Vec3 center = { 0.0f, 0.0f, 0.0f };
    Layout_AddWall3(layout, center, (Vec3){ 1.0f, 0.0f, 0.0f });
    Layout_AddWall3(layout, center, (Vec3){ 0.0f, 1.0f, 0.0f });
    Layout_AddWall3(layout, center, (Vec3){ -1.0f, 0.0f, 0.0f });

    int centerIdx = Layout_AddAnchor3(layout, center);
    TEST_ASSERT(centerIdx >= 0);
    TEST_ASSERT(layout->anchors[centerIdx].connectionCount == 3);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_compute_centroid_ignores_deleted_anchors(void) {
    ld_test_init_runtime();
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
    TEST_ASSERT(ld_test_nearly_equal(center.x, 5.0f));
    TEST_ASSERT(ld_test_nearly_equal(center.y, 1.0f));
    TEST_ASSERT(ld_test_nearly_equal(center.z, 2.0f));

    ld_test_shutdown_runtime();
    return true;
}

bool test_layout_core_run_tests(void) {
    const TestCase cases[] = {
        { "AddWallReusesAnchors", test_layout_add_wall_reuses_anchors },
        { "RemoveWallCompactsOnDemand", test_layout_remove_wall_compacts_on_demand },
        { "LayoutStringRoundtrip", test_layout_string_roundtrip },
        { "EditorUndoRedoRestoresLayout", test_editor_undo_redo_restores_layout },
        { "LayoutJsonEmbedsVersion", test_layout_json_embeds_version },
        { "LayoutJsonFutureVersionRejected", test_layout_json_future_version_rejected },
        { "LayoutJsonMissingVersionDefaults", test_layout_json_missing_version_defaults },
        { "LayoutSceneBoundsClampContract", test_layout_scene_bounds_clamp_contract },
        { "LayoutSceneBoundsFaceHandleResizeContract", test_layout_scene_bounds_face_handle_resize_contract },
        { "LayoutSceneBoundsEdgeAndCornerHandleResizeContract",
          test_layout_scene_bounds_edge_and_corner_handle_resize_contract },
        { "LayoutSceneBoundsCenterHandleTranslateContract",
          test_layout_scene_bounds_center_handle_translate_contract },
        { "LayoutFitSceneBoundsToObjectUsesWorldAABB",
          test_layout_fit_scene_bounds_to_object_uses_world_aabb },
        { "LayoutJsonV6PersistsSceneBounds", test_layout_json_v6_persists_scene_bounds },
        { "LayoutJsonMissingScene3DDefaultsSceneBounds", test_layout_json_missing_scene3d_defaults_scene_bounds },
        { "ConstructionPlaneAxisModeMapsToViewContext", test_construction_plane_axis_mode_maps_to_view_context },
        { "ConstructionPlaneCustomFrameValidationAndProjectionFallback", test_construction_plane_custom_frame_validation_and_projection_fallback },
        { "LayoutJsonV6PersistsConstructionPlaneCustomFrame", test_layout_json_v6_persists_construction_plane_custom_frame },
        { "LayoutJsonPreservesAnchorHandles", test_layout_json_preserves_anchor_handles },
        { "LayoutJsonV3PersistsAnchorZ", test_layout_json_v3_persists_anchor_z },
        { "LayoutJsonV2DefaultsZToZero", test_layout_json_v2_defaults_z_to_zero },
        { "LayoutJsonV4DefaultsZOmitted", test_layout_json_v4_defaults_z_to_zero_when_omitted },
        { "LayoutJsonAcceptsAdditiveUnknownFields", test_layout_json_accepts_additive_unknown_fields },
        { "LayoutHandlesLinkToggle", test_layout_handles_link_toggle },
        { "EditorHistoryLimitEnforced", test_editor_history_limit_enforced },
        { "LayoutAddAnchor3PreservesZ", test_layout_add_anchor3_preserves_z },
        { "LayoutCornerAnchorAllowsMoreThanTwoConnections", test_layout_corner_anchor_allows_more_than_two_connections },
        { "LayoutComputeCentroidIgnoresDeletedAnchors", test_layout_compute_centroid_ignores_deleted_anchors }
    };

    return run_test_cases("LayoutCore", cases, sizeof(cases) / sizeof(cases[0]));
}
