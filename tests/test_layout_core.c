#include "test_layout_internal.h"

#include "Editor/scene_authoring_path_handles.h"
#include "Layout/scene/layout_scene_path_edit.h"
#include "Layout/scene/layout_scene_path_geometry.h"
#include "Layout/scene/layout_scene_path_traversal.h"
#include "Layout/scene/layout_scene_camera_authoring.h"
#include "Layout/scene/layout_scene_light_authoring.h"

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
    Layout_FreeString(snapshot);

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
    Layout_FreeString(json);
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

    Layout_FreeString(baseline);
    Layout_FreeString(after);
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
    Layout_FreeString(json);
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
    Layout_FreeString(json);
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
    Layout_FreeString(snapshot);

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
    Layout_FreeString(snapshot);

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

static bool test_layout_scene_authoring_defaults_seed_authoring_entities(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    TEST_ASSERT(layout->sceneAuthoring.light_count == 1u);
    TEST_ASSERT(strcmp(layout->sceneAuthoring.lights[0].light_id, "light_key") == 0);
    TEST_ASSERT(layout->sceneAuthoring.lights[0].enabled);
    TEST_ASSERT(layout->sceneAuthoring.path_count == 2u);
    TEST_ASSERT(layout->sceneAuthoring.paths[0].role == LINE_DRAWING_SCENE_PATH_ROLE_CAMERA);
    TEST_ASSERT(layout->sceneAuthoring.paths[1].role == LINE_DRAWING_SCENE_PATH_ROLE_LIGHT);
    TEST_ASSERT(strcmp(layout->sceneAuthoring.lights[0].path_id,
                       layout->sceneAuthoring.paths[1].path_id) == 0);
    TEST_ASSERT(strcmp(layout->sceneAuthoring.paths[1].bound_light_id, "light_key") == 0);
    TEST_ASSERT(strcmp(layout->sceneAuthoring.paths[0].curve_type, "bezier") == 0);
    TEST_ASSERT(layout->sceneAuthoring.paths[0].control_point_count == 4u);
    TEST_ASSERT(layout->sceneAuthoring.material_count == 1u);
    TEST_ASSERT(strcmp(layout->sceneAuthoring.materials[0].material_id, "mat_default") == 0);
    TEST_ASSERT(layout->sceneAuthoring.selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_NONE);

    TEST_ASSERT(Layout_SceneAuthoringState_Select(&layout->sceneAuthoring,
                                                  LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT,
                                                  0u));
    TEST_ASSERT(layout->sceneAuthoring.selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT);
    TEST_ASSERT(!Layout_SceneAuthoringState_Select(&layout->sceneAuthoring,
                                                   LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT,
                                                   layout->sceneAuthoring.light_count));

    size_t index = 0u;
    TEST_ASSERT(Layout_SceneAuthoringState_AddDefaultLight(&layout->sceneAuthoring, &index));
    TEST_ASSERT(index == 1u);
    TEST_ASSERT(layout->sceneAuthoring.light_count == 2u);
    TEST_ASSERT(strcmp(layout->sceneAuthoring.lights[index].light_id, "light_002") == 0);
    TEST_ASSERT(layout->sceneAuthoring.selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT);
    TEST_ASSERT(layout->sceneAuthoring.selected_index == index);
    TEST_ASSERT(Layout_SceneAuthoringState_AddDefaultCameraPath(&layout->sceneAuthoring, &index));
    TEST_ASSERT(index == 2u);
    TEST_ASSERT(strcmp(layout->sceneAuthoring.paths[index].path_id, "path_camera_003") == 0);
    TEST_ASSERT(layout->sceneAuthoring.selected_kind ==
                LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH);
    TEST_ASSERT(Layout_SceneAuthoringState_AddDefaultMaterial(&layout->sceneAuthoring, &index));
    TEST_ASSERT(index == 1u);
    TEST_ASSERT(strcmp(layout->sceneAuthoring.materials[index].material_id, "mat_002") == 0);
    TEST_ASSERT(layout->sceneAuthoring.selected_kind ==
                LINE_DRAWING_SCENE_AUTHORING_SELECTION_MATERIAL);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_scene_authoring_path_handles_follow_selected_records(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    LineDrawingSceneAuthoringState* authoring = &state->layout.sceneAuthoring;

    TEST_ASSERT(state->editor.sceneAuthoringEditMode == SCENE_AUTHORING_EDIT_MODE_NONE);
    TEST_ASSERT(!SceneAuthoringPathHandles_ShouldShow(state));

    TEST_ASSERT(Layout_SceneAuthoringState_Select(authoring,
                                                  LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH,
                                                  0u));
    TEST_ASSERT(SceneAuthoringPathHandles_ShouldShow(state));

    TEST_ASSERT(Layout_SceneAuthoringState_Select(authoring,
                                                  LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT,
                                                  0u));
    TEST_ASSERT(SceneAuthoringPathHandles_ShouldShow(state));

    TEST_ASSERT(Layout_SceneAuthoringState_Select(authoring,
                                                  LINE_DRAWING_SCENE_AUTHORING_SELECTION_MATERIAL,
                                                  0u));
    TEST_ASSERT(!SceneAuthoringPathHandles_ShouldShow(state));

    state->workspaceMode = LINE_DRAWING_WORKSPACE_MODE_OBJECT;
    TEST_ASSERT(Layout_SceneAuthoringState_Select(authoring,
                                                  LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH,
                                                  0u));
    TEST_ASSERT(!SceneAuthoringPathHandles_ShouldShow(state));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_scene_authoring_path_handles_pick_selected_camera_path_point(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    LineDrawingSceneAuthoringState* authoring = &state->layout.sceneAuthoring;
    SpaceViewContext view_ctx = {0};
    Vec2 screen = {0};
    SceneAuthoringGizmoPickResult pick = SceneAuthoringGizmoPickResult_None();

    TEST_ASSERT(Layout_SceneAuthoringState_Select(authoring,
                                                  LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH,
                                                  0u));
    TEST_ASSERT(authoring->path_count > 0u);
    TEST_ASSERT(authoring->paths[0].control_point_count > 0u);
    TEST_ASSERT(SceneAuthoringPathHandles_ShouldShow(state));

    view_ctx = SpaceAdapter_BuildViewContext(state);
    screen = WorldToScreen(
        SpaceAdapter_ProjectToView(authoring->paths[0].control_points[0], &view_ctx),
        &state->grid);

    TEST_ASSERT(SceneAuthoringPathHandles_Pick(state,
                                               (int)lroundf(screen.x),
                                               (int)lroundf(screen.y),
                                               &pick));
    TEST_ASSERT(pick.part == SCENE_AUTHORING_GIZMO_PART_CENTER);
    TEST_ASSERT(pick.handle.kind == SCENE_AUTHORING_PATH_HANDLE_CONTROL_POINT);
    TEST_ASSERT(pick.handle.path_index == 0u);
    TEST_ASSERT(pick.handle.control_index == 0u);

    SceneAuthoringPathHandles_Select(&state->editor, pick.handle);
    TEST_ASSERT(state->editor.selectedSceneAuthoringPathIndex == 0);
    TEST_ASSERT(state->editor.selectedSceneAuthoringControlPointIndex == 0);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_scene_authoring_path_gizmo_pick_stays_screen_sized_across_zoom(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    LineDrawingSceneAuthoringState* authoring = &state->layout.sceneAuthoring;
    const float scales[] = { 2.0f, 80.0f };
    SceneAuthoringPathHandleRef selected = {
        .kind = SCENE_AUTHORING_PATH_HANDLE_CONTROL_POINT,
        .light_index = 0u,
        .path_index = 0u,
        .control_index = 0u
    };

    TEST_ASSERT(Layout_SceneAuthoringState_Select(authoring,
                                                  LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH,
                                                  0u));
    TEST_ASSERT(authoring->path_count > 0u);
    TEST_ASSERT(authoring->paths[0].control_point_count > 0u);
    SceneAuthoringPathHandles_Select(&state->editor, selected);

    for (size_t i = 0u; i < sizeof(scales) / sizeof(scales[0]); ++i) {
        SpaceViewContext view_ctx = {0};
        Vec3 origin = authoring->paths[0].control_points[0];
        Vec2 center = {0};
        Vec2 projected_x = {0};
        Vec2 direction = {0};
        float direction_length = 0.0f;
        SceneAuthoringGizmoPickResult pick = SceneAuthoringGizmoPickResult_None();

        state->grid.scale = scales[i];
        view_ctx = SpaceAdapter_BuildViewContext(state);
        center = WorldToScreen(SpaceAdapter_ProjectToView(origin, &view_ctx), &state->grid);
        projected_x = WorldToScreen(
            SpaceAdapter_ProjectToView(Vec3_Add(origin, (Vec3){1.0f, 0.0f, 0.0f}), &view_ctx),
            &state->grid);
        direction = (Vec2){ projected_x.x - center.x, projected_x.y - center.y };
        direction_length = sqrtf(direction.x * direction.x + direction.y * direction.y);
        TEST_ASSERT(direction_length > 0.001f);
        direction.x /= direction_length;
        direction.y /= direction_length;

        TEST_ASSERT(SceneAuthoringPathHandles_Pick(
            state,
            (int)lroundf(center.x + direction.x * 48.0f),
            (int)lroundf(center.y + direction.y * 48.0f),
            &pick));
        TEST_ASSERT(pick.part == SCENE_AUTHORING_GIZMO_PART_AXIS);
        TEST_ASSERT(pick.axis == GIZMO_AXIS_DIR_POS_X);
        TEST_ASSERT(pick.handle.kind == SCENE_AUTHORING_PATH_HANDLE_CONTROL_POINT);
        TEST_ASSERT(pick.handle.path_index == 0u);
        TEST_ASSERT(pick.handle.control_index == 0u);
    }

    ld_test_shutdown_runtime();
    return true;
}

static bool scene_authoring_test_axis_drag(bool light,
                                           size_t control_index,
                                           GizmoAxisDirection axis) {
    GlobalState* state = Global_Get();
    LineDrawingSceneAuthoringState* authoring = &state->layout.sceneAuthoring;
    SceneAuthoringPathHandleRef handle = {
        .kind = light ? SCENE_AUTHORING_PATH_HANDLE_LIGHT_POSITION
                      : SCENE_AUTHORING_PATH_HANDLE_CONTROL_POINT,
        .light_index = 0u,
        .path_index = 0u,
        .control_index = control_index,
        .element_kind = light ? LINE_DRAWING_SCENE_PATH_ELEMENT_NONE
                              : Layout_ScenePathEdit_ElementForControl(
                                    &authoring->paths[0], control_index).kind
    };
    Vec3* point = light ? &authoring->lights[0].position
                        : &authoring->paths[0].control_points[control_index];
    Vec3 start = {1.25f, -2.5f, 3.75f};
    SpaceViewContext view_ctx = {0};
    Vec2 center = {0};
    Vec2 projected = {0};
    Vec2 direction = {0};
    float length = 0.0f;
    SceneAuthoringGizmoPickResult pick = SceneAuthoringGizmoPickResult_None();
    size_t undo_before = Editor_UndoCount(&state->editor);

    *point = start;
    SceneAuthoringPathHandles_Select(&state->editor, handle);
    view_ctx = SpaceAdapter_BuildViewContext(state);
    center = WorldToScreen(SpaceAdapter_ProjectToView(start, &view_ctx), &state->grid);
    projected = WorldToScreen(
        SpaceAdapter_ProjectToView(Vec3_Add(start, GizmoAxisDirection_WorldVector(axis)),
                                   &view_ctx),
        &state->grid);
    direction = (Vec2){projected.x - center.x, projected.y - center.y};
    length = sqrtf(direction.x * direction.x + direction.y * direction.y);
    TEST_ASSERT(length > 0.001f);
    direction.x /= length;
    direction.y /= length;
    const int down_x = (int)lroundf(center.x + direction.x * 48.0f);
    const int down_y = (int)lroundf(center.y + direction.y * 48.0f);
    TEST_ASSERT(SceneAuthoringPathHandles_Pick(state, down_x, down_y, &pick));
    TEST_ASSERT(pick.part == SCENE_AUTHORING_GIZMO_PART_AXIS);
    TEST_ASSERT(pick.axis == axis);
    TEST_ASSERT(BeginSceneAuthoringPathHandleDragSession(state,
                                                         &state->editor,
                                                         pick,
                                                         down_x,
                                                         down_y));
    TEST_ASSERT(sceneAuthoringPathHandleDrag.pick.axis == axis);
    TEST_ASSERT(state->editor.activeSceneAuthoringGizmoAxis == (int)axis);
    UpdateSceneAuthoringPathHandleDragPosition(
        (int)lroundf((float)down_x + direction.x * 20.0f),
        (int)lroundf((float)down_y + direction.y * 20.0f));
    UpdateSceneAuthoringPathHandleDragPosition(
        (int)lroundf((float)down_x + direction.x * 30.0f),
        (int)lroundf((float)down_y + direction.y * 30.0f));
    TEST_ASSERT(Editor_UndoCount(&state->editor) == undo_before + 1u);
    if (axis == GIZMO_AXIS_DIR_POS_X) {
        TEST_ASSERT(!ld_test_nearly_equal(point->x, start.x));
        TEST_ASSERT(ld_test_nearly_equal(point->y, start.y));
        TEST_ASSERT(ld_test_nearly_equal(point->z, start.z));
    } else if (axis == GIZMO_AXIS_DIR_POS_Y) {
        TEST_ASSERT(ld_test_nearly_equal(point->x, start.x));
        TEST_ASSERT(!ld_test_nearly_equal(point->y, start.y));
        TEST_ASSERT(ld_test_nearly_equal(point->z, start.z));
    } else {
        TEST_ASSERT(ld_test_nearly_equal(point->x, start.x));
        TEST_ASSERT(ld_test_nearly_equal(point->y, start.y));
        TEST_ASSERT(!ld_test_nearly_equal(point->z, start.z));
    }
    ResetSceneAuthoringPathHandleDrag(&state->editor);
    return true;
}

static bool test_scene_authoring_axis_drags_isolate_camera_and_light_coordinates(void) {
    const GizmoAxisDirection axes[] = {
        GIZMO_AXIS_DIR_POS_X, GIZMO_AXIS_DIR_POS_Y, GIZMO_AXIS_DIR_POS_Z
    };
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    LineDrawingSceneAuthoringState* authoring = &state->layout.sceneAuthoring;
    state->freeViewCamera.enabled = true;
    state->freeViewCamera.yawDeg = 35.0f;
    state->freeViewCamera.pitchDeg = 20.0f;
    TEST_ASSERT(Layout_SceneAuthoringState_Select(authoring,
                                                  LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH,
                                                  0u));
    for (size_t i = 0u; i < 3u; ++i) {
        TEST_ASSERT(scene_authoring_test_axis_drag(false, 0u, axes[i]));
    }
    authoring->paths[0].tangent_modes[0] = LINE_DRAWING_SCENE_PATH_TANGENT_BROKEN;
    for (size_t i = 0u; i < 3u; ++i) {
        TEST_ASSERT(scene_authoring_test_axis_drag(false, 1u, axes[i]));
    }
    TEST_ASSERT(Layout_SceneAuthoringState_Select(authoring,
                                                  LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT,
                                                  0u));
    for (size_t i = 0u; i < 3u; ++i) {
        TEST_ASSERT(scene_authoring_test_axis_drag(true, 0u, axes[i]));
    }
    ld_test_shutdown_runtime();
    return true;
}

static bool test_scene_authoring_center_drag_uses_construction_plane(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    LineDrawingSceneAuthoringState* authoring = &state->layout.sceneAuthoring;
    SceneAuthoringPathHandleRef handle = {
        .kind = SCENE_AUTHORING_PATH_HANDLE_CONTROL_POINT,
        .path_index = 0u,
        .control_index = 0u
    };
    SpaceViewContext view_ctx = {0};
    Vec2 center = {0};
    Vec3 expected = {0};
    SceneAuthoringGizmoPickResult pick = SceneAuthoringGizmoPickResult_None();
    TEST_ASSERT(Layout_SceneAuthoringState_Select(authoring,
                                                  LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH,
                                                  0u));
    SceneAuthoringPathHandles_Select(&state->editor, handle);
    view_ctx = SpaceAdapter_BuildViewContext(state);
    center = WorldToScreen(SpaceAdapter_ProjectToView(authoring->paths[0].control_points[0],
                                                       &view_ctx),
                           &state->grid);
    TEST_ASSERT(SceneAuthoringPathHandles_Pick(state, (int)center.x, (int)center.y, &pick));
    TEST_ASSERT(pick.part == SCENE_AUTHORING_GIZMO_PART_CENTER);
    TEST_ASSERT(BeginSceneAuthoringPathHandleDragSession(state, &state->editor, pick,
                                                         (int)center.x, (int)center.y));
    TEST_ASSERT(SpaceAdapter_ScreenToWorld((int)center.x + 37, (int)center.y + 23,
                                           &state->grid, &view_ctx, true, &expected));
    UpdateSceneAuthoringPathHandleDragPosition((int)center.x + 37, (int)center.y + 23);
    TEST_ASSERT(ld_test_nearly_equal(authoring->paths[0].control_points[0].x, expected.x));
    TEST_ASSERT(ld_test_nearly_equal(authoring->paths[0].control_points[0].y, expected.y));
    TEST_ASSERT(ld_test_nearly_equal(authoring->paths[0].control_points[0].z, expected.z));
    ResetSceneAuthoringPathHandleDrag(&state->editor);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_scene_authoring_gizmo_pick_priority_and_degenerate_axis_policy(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    LineDrawingSceneAuthoringState* authoring = &state->layout.sceneAuthoring;
    SceneAuthoringPathHandleRef handle = {
        .kind = SCENE_AUTHORING_PATH_HANDLE_CONTROL_POINT,
        .path_index = 0u,
        .control_index = 0u
    };
    SpaceViewContext view_ctx = {0};
    Vec3 origin = authoring->paths[0].control_points[0];
    Vec2 center = {0};
    SceneAuthoringGizmoPickResult pick = SceneAuthoringGizmoPickResult_None();
    TEST_ASSERT(Layout_SceneAuthoringState_Select(authoring,
                                                  LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH,
                                                  0u));
    SceneAuthoringPathHandles_Select(&state->editor, handle);
    view_ctx = SpaceAdapter_BuildViewContext(state);
    center = WorldToScreen(SpaceAdapter_ProjectToView(origin, &view_ctx), &state->grid);
    TEST_ASSERT(SceneAuthoringPathHandles_Pick(state, (int)center.x, (int)center.y, &pick));
    TEST_ASSERT(pick.part == SCENE_AUTHORING_GIZMO_PART_CENTER);

    state->freeViewCamera.enabled = true;
    state->freeViewCamera.yawDeg = 0.0f;
    state->freeViewCamera.pitchDeg = 0.0f;
    view_ctx = SpaceAdapter_BuildViewContext(state);
    center = WorldToScreen(SpaceAdapter_ProjectToView(origin, &view_ctx), &state->grid);
    pick = SceneAuthoringGizmoPickResult_None();
    TEST_ASSERT(!SceneAuthoringPathHandles_Pick(state,
                                                (int)lroundf(center.x + 48.0f),
                                                (int)lroundf(center.y),
                                                &pick) ||
                pick.axis != GIZMO_AXIS_DIR_POS_Z);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_scene_path_geometry_evaluates_linear_and_cubic_contracts(void) {
    LineDrawingScenePath path = {0};
    LineDrawingScenePathGeometry geometry = {0};
    snprintf(path.curve_type, sizeof(path.curve_type), "linear");
    path.control_point_count = 4u;
    path.control_points[0] = (Vec3){ 0.0f, 0.0f, 0.0f };
    path.control_points[1] = (Vec3){ 0.0f, 3.0f, 0.0f };
    path.control_points[2] = (Vec3){ 3.0f, 3.0f, 0.0f };
    path.control_points[3] = (Vec3){ 3.0f, 0.0f, 0.0f };

    TEST_ASSERT(Layout_ScenePathGeometry_Build(&path, &geometry));
    TEST_ASSERT(geometry.kind == LINE_DRAWING_SCENE_PATH_GEOMETRY_LINEAR);
    TEST_ASSERT(geometry.sample_count == 4u);
    TEST_ASSERT(ld_test_vec3_nearly_equal(geometry.samples[1].world,
                                          path.control_points[1]));

    snprintf(path.curve_type, sizeof(path.curve_type), "bezier");
    TEST_ASSERT(Layout_ScenePathGeometry_Build(&path, &geometry));
    TEST_ASSERT(geometry.kind == LINE_DRAWING_SCENE_PATH_GEOMETRY_CUBIC_BEZIER);
    TEST_ASSERT(geometry.sample_count ==
                LINE_DRAWING_SCENE_PATH_CUBIC_SAMPLES_PER_SEGMENT + 1u);
    TEST_ASSERT(ld_test_vec3_nearly_equal(geometry.samples[0].world,
                                          path.control_points[0]));
    TEST_ASSERT(ld_test_vec3_nearly_equal(
        geometry.samples[geometry.sample_count - 1u].world,
        path.control_points[3]));
    TEST_ASSERT(ld_test_nearly_equal(
        geometry.samples[LINE_DRAWING_SCENE_PATH_CUBIC_SAMPLES_PER_SEGMENT / 2u].world.y,
        2.25f));
    return true;
}

static bool test_scene_path_geometry_handles_continuity_fallback_and_degeneracy(void) {
    LineDrawingScenePath path = {0};
    LineDrawingScenePathGeometry geometry = {0};
    snprintf(path.curve_type, sizeof(path.curve_type), "bezier");
    path.control_point_count = 7u;
    for (size_t i = 0u; i < path.control_point_count; ++i) {
        path.control_points[i] = (Vec3){ (float)i, i == 3u ? 2.0f : 0.0f, 1.0f };
    }
    TEST_ASSERT(Layout_ScenePathGeometry_Build(&path, &geometry));
    TEST_ASSERT(geometry.source_segment_count == 2u);
    TEST_ASSERT(geometry.sample_count ==
                2u * LINE_DRAWING_SCENE_PATH_CUBIC_SAMPLES_PER_SEGMENT + 1u);
    TEST_ASSERT(ld_test_vec3_nearly_equal(
        geometry.samples[LINE_DRAWING_SCENE_PATH_CUBIC_SAMPLES_PER_SEGMENT].world,
        path.control_points[3]));

    path.control_point_count = 3u;
    TEST_ASSERT(Layout_ScenePathGeometry_Build(&path, &geometry));
    TEST_ASSERT(geometry.kind ==
                LINE_DRAWING_SCENE_PATH_GEOMETRY_INCOMPLETE_BEZIER_FALLBACK);
    TEST_ASSERT(geometry.sample_count == 3u);

    path.control_point_count = 4u;
    for (size_t i = 0u; i < path.control_point_count; ++i) {
        path.control_points[i] = (Vec3){ 4.0f, -2.0f, 9.0f };
    }
    TEST_ASSERT(Layout_ScenePathGeometry_Build(&path, &geometry));
    for (size_t i = 0u; i < geometry.sample_count; ++i) {
        TEST_ASSERT(ld_test_vec3_nearly_equal(geometry.samples[i].world,
                                              path.control_points[0]));
    }
    return true;
}

static bool test_scene_path_geometry_cubic_split_preserves_curve(void) {
    LineDrawingScenePath path = {0};
    Vec3 original_quarter = {0};
    size_t inserted_anchor = 0u;
    snprintf(path.curve_type, sizeof(path.curve_type), "bezier");
    path.control_point_count = 4u;
    path.control_points[0] = (Vec3){ 0.0f, 0.0f, 0.0f };
    path.control_points[1] = (Vec3){ 0.0f, 4.0f, 0.0f };
    path.control_points[2] = (Vec3){ 4.0f, 4.0f, 0.0f };
    path.control_points[3] = (Vec3){ 4.0f, 0.0f, 0.0f };
    original_quarter = Layout_ScenePathGeometry_EvaluateCubic(
        path.control_points[0], path.control_points[1],
        path.control_points[2], path.control_points[3], 0.25f);
    TEST_ASSERT(Layout_ScenePathGeometry_SplitCubicSegment(&path, 0u, 0.5f,
                                                           &inserted_anchor));
    TEST_ASSERT(path.control_point_count == 7u);
    TEST_ASSERT(inserted_anchor == 3u);
    TEST_ASSERT(Layout_ScenePathGeometry_IsCompleteCubic(&path));
    TEST_ASSERT(ld_test_vec3_nearly_equal(
        Layout_ScenePathGeometry_EvaluateCubic(path.control_points[0],
                                               path.control_points[1],
                                               path.control_points[2],
                                               path.control_points[3],
                                               0.5f),
        original_quarter));
    return true;
}

static bool test_scene_path_traversal_uses_world_distance_and_loop_policy(void) {
    LineDrawingScenePath path = {0};
    LineDrawingScenePathTraversalTable table = {0};
    LineDrawingScenePathTraversalSample sample = {0};
    snprintf(path.curve_type, sizeof(path.curve_type), "linear");
    path.control_point_count = 3u;
    path.control_points[0] = (Vec3){0.0f, 0.0f, 0.0f};
    path.control_points[1] = (Vec3){1.0f, 0.0f, 0.0f};
    path.control_points[2] = (Vec3){10.0f, 0.0f, 0.0f};
    path.duration_seconds = 5.0f;
    TEST_ASSERT(Layout_ScenePathTraversal_Build(&path, &table));
    TEST_ASSERT(fabsf(table.total_distance - 10.0f) < 0.0001f);
    TEST_ASSERT(Layout_ScenePathTraversal_EvaluateNormalized(
        &table, 0.5f, LINE_DRAWING_SCENE_PATH_PLAYBACK_ONCE, &sample));
    TEST_ASSERT(fabsf(sample.world.x - 5.0f) < 0.0001f);
    TEST_ASSERT(Layout_ScenePathTraversal_EvaluateTime(
        &table, 2.5f, path.duration_seconds, LINE_DRAWING_SCENE_PATH_PLAYBACK_ONCE, &sample));
    TEST_ASSERT(fabsf(sample.world.x - 5.0f) < 0.0001f);
    TEST_ASSERT(Layout_ScenePathTraversal_EvaluateNormalized(
        &table, 1.25f, LINE_DRAWING_SCENE_PATH_PLAYBACK_LOOP, &sample));
    TEST_ASSERT(fabsf(sample.world.x - 2.5f) < 0.0001f);
    path.closed = true;
    TEST_ASSERT(Layout_ScenePathTraversal_Build(&path, &table));
    TEST_ASSERT(fabsf(table.total_distance - 20.0f) < 0.0001f);
    TEST_ASSERT(Layout_ScenePathTraversal_EvaluateNormalized(
        &table, 0.75f, LINE_DRAWING_SCENE_PATH_PLAYBACK_ONCE, &sample));
    TEST_ASSERT(fabsf(sample.world.x - 5.0f) < 0.0001f);
    {
        LineDrawingScenePath a = path;
        LineDrawingScenePath b = path;
        a.closed = false;
        b.closed = false;
        a.playing = true;
        b.playing = true;
        a.normalized_distance = 0.0f;
        b.normalized_distance = 0.0f;
        a.duration_seconds = 4.0f;
        b.duration_seconds = 4.0f;
        for (int i = 0; i < 10; ++i) TEST_ASSERT(Layout_ScenePathTraversal_Advance(&a, 0.1f));
        for (int i = 0; i < 4; ++i) TEST_ASSERT(Layout_ScenePathTraversal_Advance(&b, 0.25f));
        TEST_ASSERT(fabsf(a.normalized_distance - b.normalized_distance) < 0.0001f);
        TEST_ASSERT(fabsf(a.normalized_distance - 0.25f) < 0.0001f);
    }
    return true;
}

static bool test_scene_path_traversal_drives_camera_and_light_followers(void) {
    LineDrawingScenePath path = {0};
    LineDrawingSceneCamera camera = {0};
    LineDrawingSceneCameraPose pose = {0};
    LineDrawingSceneLight light = {0};
    Vec3 light_position = {0};
    snprintf(path.curve_type, sizeof(path.curve_type), "linear");
    path.control_point_count = 3u;
    path.control_points[0] = (Vec3){0.0f, 0.0f, 1.0f};
    path.control_points[1] = (Vec3){1.0f, 0.0f, 1.0f};
    path.control_points[2] = (Vec3){9.0f, 0.0f, 1.0f};
    path.duration_seconds = 3.0f;
    Layout_SceneCamera_SetDefaults(&camera, "camera_loaded", "Loaded Camera", "path_loaded");
    camera.orientation_mode = LINE_DRAWING_SCENE_CAMERA_ORIENTATION_LOOK_AT_TARGET;
    camera.look_at_target = (Vec3){4.5f, 4.0f, 1.0f};
    TEST_ASSERT(Layout_SceneCamera_EvaluatePoseAtNormalizedDistance(&camera, &path, 0.5f, &pose));
    TEST_ASSERT(fabsf(pose.position.x - 4.5f) < 0.0001f);
    Layout_SceneLight_SetDefaults(&light, "light_loaded", "Loaded Light");
    light.position_mode = LINE_DRAWING_SCENE_LIGHT_POSITION_PATH_START;
    TEST_ASSERT(Layout_SceneLight_EvaluatePositionAtNormalizedDistance(&light,
                                                                       &path,
                                                                       0.5f,
                                                                       &light_position));
    TEST_ASSERT(fabsf(light_position.x - 4.5f) < 0.0001f);
    TEST_ASSERT(fabsf(light_position.z - 1.0f) < 0.0001f);
    return true;
}

static bool test_scene_path_edit_classifies_typed_elements_and_modes(void) {
    LineDrawingScenePath path = {0};
    snprintf(path.curve_type, sizeof(path.curve_type), "bezier");
    path.control_point_count = 7u;
    for (size_t i = 0u; i < path.control_point_count; ++i) {
        path.control_points[i] = (Vec3){(float)i, 0.0f, 0.0f};
    }
    TEST_ASSERT(Layout_ScenePathEdit_AnchorCount(&path) == 3u);
    TEST_ASSERT(Layout_ScenePathEdit_ElementForControl(&path, 0u).kind ==
                LINE_DRAWING_SCENE_PATH_ELEMENT_ANCHOR);
    TEST_ASSERT(Layout_ScenePathEdit_ElementForControl(&path, 1u).kind ==
                LINE_DRAWING_SCENE_PATH_ELEMENT_OUTGOING_TANGENT);
    TEST_ASSERT(Layout_ScenePathEdit_ElementForControl(&path, 2u).kind ==
                LINE_DRAWING_SCENE_PATH_ELEMENT_INCOMING_TANGENT);
    TEST_ASSERT(Layout_ScenePathEdit_ElementForControl(&path, 3u).anchor_index == 1u);
    TEST_ASSERT(Layout_ScenePathEdit_Segment(1u).kind ==
                LINE_DRAWING_SCENE_PATH_ELEMENT_SEGMENT);

    path.control_points[2] = (Vec3){2.0f, 0.0f, 0.0f};
    path.control_points[3] = (Vec3){3.0f, 0.0f, 0.0f};
    path.control_points[4] = (Vec3){4.0f, 0.0f, 0.0f};
    TEST_ASSERT(Layout_ScenePathEdit_SetAnchorMode(
        &path, 1u, LINE_DRAWING_SCENE_PATH_TANGENT_LINKED));
    TEST_ASSERT(Layout_ScenePathEdit_SetElementWorldPoint(
        &path, Layout_ScenePathEdit_ElementForControl(&path, 2u),
        (Vec3){2.0f, 2.0f, 0.0f}));
    TEST_ASSERT(ld_test_vec3_nearly_equal(path.control_points[4],
                                          (Vec3){4.0f, -2.0f, 0.0f}));

    path.control_points[4] = (Vec3){6.0f, 0.0f, 0.0f};
    TEST_ASSERT(Layout_ScenePathEdit_SetAnchorMode(
        &path, 1u, LINE_DRAWING_SCENE_PATH_TANGENT_SMOOTH));
    TEST_ASSERT(Layout_ScenePathEdit_SetElementWorldPoint(
        &path, Layout_ScenePathEdit_ElementForControl(&path, 2u),
        (Vec3){3.0f, 1.0f, 0.0f}));
    TEST_ASSERT(ld_test_nearly_equal(Vec3_Length(Vec3_Sub(path.control_points[4],
                                                          path.control_points[3])),
                                     3.0f));
    TEST_ASSERT(ld_test_nearly_equal(path.control_points[4].x, 3.0f));
    TEST_ASSERT(path.control_points[4].y < 0.0f);

    TEST_ASSERT(Layout_ScenePathEdit_SetAnchorMode(
        &path, 1u, LINE_DRAWING_SCENE_PATH_TANGENT_BROKEN));
    Vec3 opposite_before = path.control_points[4];
    TEST_ASSERT(Layout_ScenePathEdit_SetElementWorldPoint(
        &path, Layout_ScenePathEdit_ElementForControl(&path, 2u),
        (Vec3){1.0f, 4.0f, 0.0f}));
    TEST_ASSERT(ld_test_vec3_nearly_equal(path.control_points[4], opposite_before));

    TEST_ASSERT(Layout_ScenePathEdit_SetAnchorMode(
        &path, 1u, LINE_DRAWING_SCENE_PATH_TANGENT_CORNER));
    TEST_ASSERT(ld_test_vec3_nearly_equal(path.control_points[2], path.control_points[3]));
    TEST_ASSERT(ld_test_vec3_nearly_equal(path.control_points[4], path.control_points[3]));
    TEST_ASSERT(Layout_ScenePathEdit_SetAnchorMode(
        &path, 1u, LINE_DRAWING_SCENE_PATH_TANGENT_AUTOMATIC));
    TEST_ASSERT(path.control_points[2].x < path.control_points[3].x);
    TEST_ASSERT(path.control_points[4].x > path.control_points[3].x);
    TEST_ASSERT(ld_test_nearly_equal(path.control_points[2].y, 0.0f));
    TEST_ASSERT(strcmp(Layout_ScenePathTangentMode_Name(
                           LINE_DRAWING_SCENE_PATH_TANGENT_AUTOMATIC),
                       "automatic") == 0);
    return true;
}

static bool test_scene_path_edit_split_and_safe_delete_preserve_contract(void) {
    LineDrawingScenePath path = {0};
    LineDrawingScenePathElementRef inserted = {0};
    LineDrawingScenePathElementRef selected = {0};
    snprintf(path.curve_type, sizeof(path.curve_type), "bezier");
    path.control_point_count = 7u;
    for (size_t i = 0u; i < path.control_point_count; ++i) {
        path.control_points[i] = (Vec3){(float)i, i == 1u || i == 5u ? 2.0f : 0.0f, 0.0f};
    }
    path.tangent_modes[0] = LINE_DRAWING_SCENE_PATH_TANGENT_LINKED;
    path.tangent_modes[1] = LINE_DRAWING_SCENE_PATH_TANGENT_BROKEN;
    path.tangent_modes[2] = LINE_DRAWING_SCENE_PATH_TANGENT_CORNER;
    TEST_ASSERT(Layout_ScenePathEdit_SplitSegment(&path, 0u, 0.5f, &inserted));
    TEST_ASSERT(path.control_point_count == 10u);
    TEST_ASSERT(inserted.kind == LINE_DRAWING_SCENE_PATH_ELEMENT_ANCHOR);
    TEST_ASSERT(inserted.anchor_index == 1u);
    TEST_ASSERT(path.tangent_modes[0] == LINE_DRAWING_SCENE_PATH_TANGENT_LINKED);
    TEST_ASSERT(path.tangent_modes[1] == LINE_DRAWING_SCENE_PATH_TANGENT_SMOOTH);
    TEST_ASSERT(path.tangent_modes[2] == LINE_DRAWING_SCENE_PATH_TANGENT_BROKEN);
    TEST_ASSERT(path.tangent_modes[3] == LINE_DRAWING_SCENE_PATH_TANGENT_CORNER);

    TEST_ASSERT(Layout_ScenePathEdit_DeleteElement(
        &path, Layout_ScenePathEdit_ElementForControl(&path, 3u), &selected));
    TEST_ASSERT(path.control_point_count == 7u);
    TEST_ASSERT(Layout_ScenePathGeometry_IsCompleteCubic(&path));
    TEST_ASSERT(selected.kind == LINE_DRAWING_SCENE_PATH_ELEMENT_ANCHOR);

    Vec3 anchor = path.control_points[3];
    TEST_ASSERT(Layout_ScenePathEdit_DeleteElement(
        &path, Layout_ScenePathEdit_ElementForControl(&path, 2u), &selected));
    TEST_ASSERT(ld_test_vec3_nearly_equal(path.control_points[2], anchor));
    TEST_ASSERT(path.tangent_modes[1] == LINE_DRAWING_SCENE_PATH_TANGENT_BROKEN);
    TEST_ASSERT(Layout_ScenePathEdit_DeleteElement(
        &path, Layout_ScenePathEdit_Segment(0u), &selected));
    TEST_ASSERT(path.control_point_count == 4u);
    TEST_ASSERT(Layout_ScenePathGeometry_IsCompleteCubic(&path));
    TEST_ASSERT(!Layout_ScenePathEdit_DeleteElement(
        &path, Layout_ScenePathEdit_Segment(0u), &selected));
    return true;
}

static bool test_scene_authoring_typed_tangent_pick_survives_drag_session(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    LineDrawingSceneAuthoringState* authoring = &state->layout.sceneAuthoring;
    LineDrawingScenePath* path = &authoring->paths[0];
    SpaceViewContext view_ctx = SpaceAdapter_BuildViewContext(state);
    Vec2 tangent_screen = {0};
    SceneAuthoringGizmoPickResult pick = SceneAuthoringGizmoPickResult_None();
    LineDrawingScenePathGeometry geometry = {0};
    TEST_ASSERT(Layout_SceneAuthoringState_Select(authoring,
                                                  LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH,
                                                  0u));
    tangent_screen = WorldToScreen(
        SpaceAdapter_ProjectToView(path->control_points[1], &view_ctx), &state->grid);
    TEST_ASSERT(SceneAuthoringPathHandles_Pick(state,
                                                (int)lroundf(tangent_screen.x),
                                                (int)lroundf(tangent_screen.y),
                                                &pick));
    TEST_ASSERT(pick.handle.element_kind ==
                LINE_DRAWING_SCENE_PATH_ELEMENT_OUTGOING_TANGENT);
    TEST_ASSERT(pick.part == SCENE_AUTHORING_GIZMO_PART_CENTER);
    TEST_ASSERT(BeginSceneAuthoringPathHandleDragSession(state, &state->editor, pick,
                                                         (int)lroundf(tangent_screen.x),
                                                         (int)lroundf(tangent_screen.y)));
    TEST_ASSERT(sceneAuthoringPathHandleDrag.pick.handle.element_kind ==
                LINE_DRAWING_SCENE_PATH_ELEMENT_OUTGOING_TANGENT);
    TEST_ASSERT(state->editor.selectedSceneAuthoringPathElementKind ==
                LINE_DRAWING_SCENE_PATH_ELEMENT_OUTGOING_TANGENT);
    TEST_ASSERT(state->editor.selectedSceneAuthoringControlPointIndex == 1);
    ResetSceneAuthoringPathHandleDrag(&state->editor);
    SceneAuthoringPathHandles_Select(&state->editor, SceneAuthoringPathHandleRef_None());
    TEST_ASSERT(Layout_ScenePathGeometry_Build(path, &geometry));
    {
        const Vec2 segment_screen = WorldToScreen(
            SpaceAdapter_ProjectToView(
                geometry.samples[LINE_DRAWING_SCENE_PATH_CUBIC_SAMPLES_PER_SEGMENT / 2u].world,
                &view_ctx),
            &state->grid);
        pick = SceneAuthoringGizmoPickResult_None();
        TEST_ASSERT(SceneAuthoringPathHandles_Pick(state,
                                                    (int)lroundf(segment_screen.x),
                                                    (int)lroundf(segment_screen.y),
                                                    &pick));
        TEST_ASSERT(pick.handle.element_kind == LINE_DRAWING_SCENE_PATH_ELEMENT_SEGMENT);
        TEST_ASSERT(!BeginSceneAuthoringPathHandleDragSession(
            state, &state->editor, pick,
            (int)lroundf(segment_screen.x), (int)lroundf(segment_screen.y)));
        TEST_ASSERT(state->editor.selectedSceneAuthoringPathElementKind ==
                    LINE_DRAWING_SCENE_PATH_ELEMENT_SEGMENT);
        TEST_ASSERT(state->editor.selectedSceneAuthoringPathSegmentIndex == 0);
    }
    ld_test_shutdown_runtime();
    return true;
}

static bool test_scene_camera_records_modes_pose_and_delete(void) {
    ld_test_init_runtime();
    LineDrawingSceneAuthoringState* authoring = &Global_Get()->layout.sceneAuthoring;
    LineDrawingSceneCameraPose pose = {0};
    size_t path_index = 0u;
    TEST_ASSERT(authoring->camera_count == 1u);
    TEST_ASSERT(strcmp(authoring->cameras[0].camera_id, "camera_main") == 0);
    TEST_ASSERT(strcmp(authoring->cameras[0].path_id, "path_camera_main") == 0);
    TEST_ASSERT(authoring->cameras[0].orientation_mode ==
                LINE_DRAWING_SCENE_CAMERA_ORIENTATION_PATH_FACING);
    TEST_ASSERT(Layout_SceneCamera_EvaluatePose(&authoring->cameras[0],
                                                &authoring->paths[0], &pose));
    TEST_ASSERT(ld_test_vec3_nearly_equal(pose.position,
                                          authoring->paths[0].control_points[0]));
    TEST_ASSERT(Vec3_Length(pose.forward) > 0.99f);

    TEST_ASSERT(Layout_SceneAuthoringState_Select(authoring,
                                                  LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH,
                                                  0u));
    TEST_ASSERT(Layout_SceneAuthoringState_CycleSelectedCameraOrientation(authoring));
    TEST_ASSERT(authoring->cameras[0].orientation_mode ==
                LINE_DRAWING_SCENE_CAMERA_ORIENTATION_LOOK_AT_TARGET);
    authoring->cameras[0].look_at_target = Vec3_Add(pose.position, (Vec3){0.0f, 4.0f, 0.0f});
    TEST_ASSERT(Layout_SceneCamera_EvaluatePose(&authoring->cameras[0],
                                                &authoring->paths[0], &pose));
    TEST_ASSERT(pose.forward.y > 0.99f);
    TEST_ASSERT(Layout_SceneAuthoringState_CycleSelectedCameraOrientation(authoring));
    TEST_ASSERT(authoring->cameras[0].orientation_mode ==
                LINE_DRAWING_SCENE_CAMERA_ORIENTATION_FIXED);
    TEST_ASSERT(Layout_SceneAuthoringState_CycleSelectedCameraOrientation(authoring));
    TEST_ASSERT(authoring->cameras[0].orientation_mode ==
                LINE_DRAWING_SCENE_CAMERA_ORIENTATION_PER_POINT);
    TEST_ASSERT(Layout_SceneAuthoringState_CycleSelectedCameraOrientation(authoring));
    TEST_ASSERT(authoring->cameras[0].orientation_mode ==
                LINE_DRAWING_SCENE_CAMERA_ORIENTATION_PATH_FACING);
    TEST_ASSERT(Layout_SceneAuthoringState_CycleSelectedCameraRoll(authoring));
    TEST_ASSERT(ld_test_nearly_equal(authoring->cameras[0].roll_degrees, 15.0f));
    TEST_ASSERT(Layout_SceneAuthoringState_CycleSelectedCameraFov(authoring));
    TEST_ASSERT(ld_test_nearly_equal(authoring->cameras[0].vertical_fov_degrees, 65.0f));
    TEST_ASSERT(Layout_SceneAuthoringState_CycleSelectedCameraClipPreset(authoring));
    TEST_ASSERT(ld_test_nearly_equal(authoring->cameras[0].near_clip, 0.5f));
    TEST_ASSERT(ld_test_nearly_equal(authoring->cameras[0].far_clip, 1000.0f));

    TEST_ASSERT(Layout_SceneAuthoringState_AddDefaultCameraPath(authoring, &path_index));
    TEST_ASSERT(authoring->camera_count == 2u);
    TEST_ASSERT(strcmp(authoring->cameras[1].path_id,
                       authoring->paths[path_index].path_id) == 0);
    TEST_ASSERT(Layout_SceneAuthoringState_DeleteSelected(authoring));
    TEST_ASSERT(authoring->camera_count == 1u);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_scene_camera_aim_pick_survives_axis_drag(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    LineDrawingSceneAuthoringState* authoring = &state->layout.sceneAuthoring;
    LineDrawingSceneCamera* camera = &authoring->cameras[0];
    SceneAuthoringPathHandleRef handle = {
        .kind = SCENE_AUTHORING_PATH_HANDLE_CAMERA_AIM,
        .camera_index = 0u,
        .path_index = 0u
    };
    SceneAuthoringGizmoPickResult pick = SceneAuthoringGizmoPickResult_None();
    SpaceViewContext view_ctx;
    Vec3 start = {2.0f, 1.0f, 3.0f};
    Vec2 center;
    Vec2 projected;
    Vec2 direction;
    float length;
    size_t undo_before;
    TEST_ASSERT(Layout_SceneAuthoringState_Select(authoring,
                                                  LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH,
                                                  0u));
    camera->orientation_mode = LINE_DRAWING_SCENE_CAMERA_ORIENTATION_LOOK_AT_TARGET;
    camera->look_at_target = start;
    SceneAuthoringPathHandles_Select(&state->editor, handle);
    TEST_ASSERT(state->editor.selectedSceneAuthoringCameraAim);
    view_ctx = SpaceAdapter_BuildViewContext(state);
    center = WorldToScreen(SpaceAdapter_ProjectToView(start, &view_ctx), &state->grid);
    projected = WorldToScreen(
        SpaceAdapter_ProjectToView(Vec3_Add(start, (Vec3){1.0f, 0.0f, 0.0f}), &view_ctx),
        &state->grid);
    direction = (Vec2){projected.x - center.x, projected.y - center.y};
    length = sqrtf(direction.x * direction.x + direction.y * direction.y);
    TEST_ASSERT(length > 0.001f);
    direction.x /= length;
    direction.y /= length;
    const int down_x = (int)lroundf(center.x + direction.x * 48.0f);
    const int down_y = (int)lroundf(center.y + direction.y * 48.0f);
    TEST_ASSERT(SceneAuthoringPathHandles_Pick(state, down_x, down_y, &pick));
    TEST_ASSERT(pick.handle.kind == SCENE_AUTHORING_PATH_HANDLE_CAMERA_AIM);
    TEST_ASSERT(pick.part == SCENE_AUTHORING_GIZMO_PART_AXIS);
    TEST_ASSERT(pick.axis == GIZMO_AXIS_DIR_POS_X);
    TEST_ASSERT(BeginSceneAuthoringPathHandleDragSession(state, &state->editor, pick,
                                                         down_x, down_y));
    TEST_ASSERT(sceneAuthoringPathHandleDrag.pick.handle.kind ==
                SCENE_AUTHORING_PATH_HANDLE_CAMERA_AIM);
    undo_before = Editor_UndoCount(&state->editor);
    UpdateSceneAuthoringPathHandleDragPosition(
        (int)lroundf((float)down_x + direction.x * 30.0f),
        (int)lroundf((float)down_y + direction.y * 30.0f));
    TEST_ASSERT(Editor_UndoCount(&state->editor) == undo_before + 1u);
    TEST_ASSERT(!ld_test_nearly_equal(camera->look_at_target.x, start.x));
    TEST_ASSERT(ld_test_nearly_equal(camera->look_at_target.y, start.y));
    TEST_ASSERT(ld_test_nearly_equal(camera->look_at_target.z, start.z));
    ResetSceneAuthoringPathHandleDrag(&state->editor);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_scene_light_aim_pick_survives_axis_drag(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    LineDrawingSceneAuthoringState* authoring = &state->layout.sceneAuthoring;
    LineDrawingSceneLight* light = &authoring->lights[0];
    SceneAuthoringPathHandleRef handle = {
        .kind = SCENE_AUTHORING_PATH_HANDLE_LIGHT_AIM,
        .light_index = 0u,
        .path_index = 1u
    };
    SceneAuthoringGizmoPickResult pick = SceneAuthoringGizmoPickResult_None();
    SpaceViewContext view_ctx;
    Vec3 start = {5.0f, 1.0f, 3.0f};
    Vec2 center;
    Vec2 projected;
    Vec2 direction;
    float length;
    size_t undo_before;
    TEST_ASSERT(Layout_SceneAuthoringState_Select(authoring,
                                                  LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT,
                                                  0u));
    light->kind = LINE_DRAWING_SCENE_LIGHT_SPOT;
    light->position_mode = LINE_DRAWING_SCENE_LIGHT_POSITION_INDEPENDENT;
    TEST_ASSERT(Layout_SceneLight_SetAimPoint(light, &authoring->paths[1], start));
    SceneAuthoringPathHandles_Select(&state->editor, handle);
    TEST_ASSERT(state->editor.selectedSceneAuthoringLightAim);
    view_ctx = SpaceAdapter_BuildViewContext(state);
    center = WorldToScreen(SpaceAdapter_ProjectToView(start, &view_ctx), &state->grid);
    projected = WorldToScreen(
        SpaceAdapter_ProjectToView(Vec3_Add(start, (Vec3){1.0f, 0.0f, 0.0f}), &view_ctx),
        &state->grid);
    direction = (Vec2){projected.x - center.x, projected.y - center.y};
    length = sqrtf(direction.x * direction.x + direction.y * direction.y);
    TEST_ASSERT(length > 0.001f);
    direction.x /= length;
    direction.y /= length;
    const int down_x = (int)lroundf(center.x + direction.x * 48.0f);
    const int down_y = (int)lroundf(center.y + direction.y * 48.0f);
    TEST_ASSERT(SceneAuthoringPathHandles_Pick(state, down_x, down_y, &pick));
    TEST_ASSERT(pick.handle.kind == SCENE_AUTHORING_PATH_HANDLE_LIGHT_AIM);
    TEST_ASSERT(pick.part == SCENE_AUTHORING_GIZMO_PART_AXIS);
    TEST_ASSERT(pick.axis == GIZMO_AXIS_DIR_POS_X);
    TEST_ASSERT(BeginSceneAuthoringPathHandleDragSession(state, &state->editor, pick,
                                                         down_x, down_y));
    TEST_ASSERT(sceneAuthoringPathHandleDrag.pick.handle.kind ==
                SCENE_AUTHORING_PATH_HANDLE_LIGHT_AIM);
    TEST_ASSERT(sceneAuthoringPathHandleDrag.pick.axis == GIZMO_AXIS_DIR_POS_X);
    undo_before = Editor_UndoCount(&state->editor);
    UpdateSceneAuthoringPathHandleDragPosition(
        (int)lroundf((float)down_x + direction.x * 30.0f),
        (int)lroundf((float)down_y + direction.y * 30.0f));
    TEST_ASSERT(Editor_UndoCount(&state->editor) == undo_before + 1u);
    TEST_ASSERT(!ld_test_nearly_equal(light->aim_target.x, start.x));
    TEST_ASSERT(ld_test_nearly_equal(light->aim_target.y, start.y));
    TEST_ASSERT(ld_test_nearly_equal(light->aim_target.z, start.z));
    ResetSceneAuthoringPathHandleDrag(&state->editor);
    ld_test_shutdown_runtime();
    return true;
}

static bool test_scene_authoring_bezier_screen_insertion_uses_sampled_geometry(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    LineDrawingSceneAuthoringState* authoring = &state->layout.sceneAuthoring;
    SpaceViewContext view_ctx = SpaceAdapter_BuildViewContext(state);
    LineDrawingScenePathGeometry geometry = {0};
    SceneAuthoringPathHandleRef inserted = SceneAuthoringPathHandleRef_None();
    Vec2 screen = {0};
    TEST_ASSERT(Layout_SceneAuthoringState_Select(authoring,
                                                  LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH,
                                                  0u));
    TEST_ASSERT(Layout_ScenePathGeometry_Build(&authoring->paths[0], &geometry));
    TEST_ASSERT(geometry.kind == LINE_DRAWING_SCENE_PATH_GEOMETRY_CUBIC_BEZIER);
    screen = WorldToScreen(
        SpaceAdapter_ProjectToView(
            geometry.samples[LINE_DRAWING_SCENE_PATH_CUBIC_SAMPLES_PER_SEGMENT / 2u].world,
            &view_ctx),
        &state->grid);
    TEST_ASSERT(SceneAuthoringPathHandles_InsertControlPointAtScreen(
        state, &state->editor, (int)lroundf(screen.x), (int)lroundf(screen.y), &inserted));
    TEST_ASSERT(authoring->paths[0].control_point_count == 7u);
    TEST_ASSERT(inserted.control_index == 3u);
    TEST_ASSERT(Layout_ScenePathGeometry_IsCompleteCubic(&authoring->paths[0]));
    ld_test_shutdown_runtime();
    return true;
}

static bool test_scene_authoring_deletes_selected_records_and_clears_references(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    LineDrawingSceneAuthoringState* authoring = &state->layout.sceneAuthoring;
    size_t light_index = 0u;
    size_t path_index = 0u;
    size_t material_index = 0u;

    TEST_ASSERT(Layout_SceneAuthoringState_AddDefaultLight(authoring, &light_index));
    TEST_ASSERT(Layout_SceneAuthoringState_AddDefaultCameraPath(authoring, &path_index));
    TEST_ASSERT(Layout_SceneAuthoringState_AddDefaultMaterial(authoring, &material_index));
    TEST_ASSERT(light_index == 1u);
    TEST_ASSERT(path_index == 2u);
    TEST_ASSERT(material_index == 1u);

    snprintf(authoring->lights[light_index].path_id,
             sizeof(authoring->lights[light_index].path_id),
             "%s",
             authoring->paths[path_index].path_id);
    snprintf(authoring->paths[path_index].bound_light_id,
             sizeof(authoring->paths[path_index].bound_light_id),
             "%s",
             authoring->lights[light_index].light_id);

    TEST_ASSERT(Layout_SceneAuthoringState_Select(authoring,
                                                  LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT,
                                                  light_index));
    TEST_ASSERT(Layout_SceneAuthoringState_DeleteSelected(authoring));
    TEST_ASSERT(authoring->light_count == 1u);
    TEST_ASSERT(authoring->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_NONE);
    TEST_ASSERT(authoring->paths[path_index].bound_light_id[0] == '\0');

    snprintf(authoring->lights[0].path_id,
             sizeof(authoring->lights[0].path_id),
             "%s",
             authoring->paths[path_index].path_id);
    TEST_ASSERT(Layout_SceneAuthoringState_Select(authoring,
                                                  LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH,
                                                  path_index));
    TEST_ASSERT(Layout_SceneAuthoringState_DeleteSelected(authoring));
    TEST_ASSERT(authoring->path_count == 2u);
    TEST_ASSERT(authoring->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_NONE);
    TEST_ASSERT(authoring->lights[0].path_id[0] == '\0');

    TEST_ASSERT(Layout_SceneAuthoringState_Select(authoring,
                                                  LINE_DRAWING_SCENE_AUTHORING_SELECTION_MATERIAL,
                                                  material_index));
    TEST_ASSERT(Layout_SceneAuthoringState_DeleteSelected(authoring));
    TEST_ASSERT(authoring->material_count == 1u);
    TEST_ASSERT(authoring->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_NONE);
    TEST_ASSERT(!Layout_SceneAuthoringState_DeleteSelected(authoring));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_json_persists_scene_authoring_records(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;
    LineDrawingSceneAuthoringState* authoring = &layout->sceneAuthoring;
    size_t light_index = 0u;
    size_t path_index = 0u;
    size_t generic_path_index = 0u;
    size_t material_index = 0u;
    Vec3 edited_point = { 2.5f, -3.25f, 4.75f };

    TEST_ASSERT(Layout_SceneAuthoringState_AddDefaultLight(authoring, &light_index));
    TEST_ASSERT(Layout_SceneAuthoringState_AddDefaultCameraPath(authoring, &path_index));
    TEST_ASSERT(Layout_SceneAuthoringState_AddDefaultGenericPath(authoring, &generic_path_index));
    TEST_ASSERT(Layout_SceneAuthoringState_AddDefaultMaterial(authoring, &material_index));
    TEST_ASSERT(light_index == 1u);
    TEST_ASSERT(path_index == 2u);
    TEST_ASSERT(generic_path_index == 3u);
    TEST_ASSERT(material_index == 1u);

    authoring->lights[light_index].kind = LINE_DRAWING_SCENE_LIGHT_SPOT;
    authoring->lights[light_index].enabled = false;
    authoring->lights[light_index].position = (Vec3){ 1.0f, 2.0f, 3.0f };
    authoring->lights[light_index].direction = (Vec3){ -0.25f, 0.5f, -1.0f };
    authoring->lights[light_index].aim_target = (Vec3){ 5.0f, 6.0f, 7.0f };
    authoring->lights[light_index].position_mode =
        LINE_DRAWING_SCENE_LIGHT_POSITION_PATH_START;
    authoring->lights[light_index].color_rgb[0] = 1.0f;
    authoring->lights[light_index].color_rgb[1] = 0.78f;
    authoring->lights[light_index].color_rgb[2] = 0.55f;
    authoring->lights[light_index].intensity = 5.0f;
    authoring->lights[light_index].radius = 1.0f;
    authoring->lights[light_index].area_size = (Vec2){4.0f, 8.0f};
    authoring->lights[light_index].inner_cone_degrees = 35.0f;
    authoring->lights[light_index].outer_cone_degrees = 60.0f;
    authoring->lights[light_index].falloff = LINE_DRAWING_SCENE_LIGHT_FALLOFF_LINEAR;
    snprintf(authoring->lights[light_index].path_id,
             sizeof(authoring->lights[light_index].path_id),
             "%s",
             authoring->paths[path_index].path_id);
    snprintf(authoring->paths[path_index].bound_light_id,
             sizeof(authoring->paths[path_index].bound_light_id),
             "%s",
             authoring->lights[light_index].light_id);
    TEST_ASSERT(Layout_SceneAuthoringState_SetPathControlPoint(authoring,
                                                                     path_index,
                                                                     1u,
                                                                     edited_point));
    authoring->paths[path_index].tangent_modes[0] =
        LINE_DRAWING_SCENE_PATH_TANGENT_BROKEN;
    authoring->cameras[1].orientation_mode =
        LINE_DRAWING_SCENE_CAMERA_ORIENTATION_LOOK_AT_TARGET;
    authoring->cameras[1].look_at_target = (Vec3){4.0f, 5.0f, 6.0f};
    authoring->cameras[1].roll_degrees = 15.0f;
    authoring->cameras[1].vertical_fov_degrees = 65.0f;
    authoring->cameras[1].near_clip = 0.5f;
    authoring->cameras[1].far_clip = 1000.0f;
    TEST_ASSERT(Layout_SceneAuthoringState_Select(authoring,
                                                  LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH,
                                                  path_index));
    TEST_ASSERT(Layout_SceneAuthoringState_CycleSelectedPathCurveType(authoring));
    TEST_ASSERT(Layout_SceneAuthoringState_Select(authoring,
                                                  LINE_DRAWING_SCENE_AUTHORING_SELECTION_MATERIAL,
                                                  material_index));
    TEST_ASSERT(Layout_SceneAuthoringState_CycleSelectedMaterialColor(authoring));

    char* json = Layout_SaveToString(layout);
    TEST_ASSERT(json != NULL);
    {
        cJSON* root = cJSON_Parse(json);
        const cJSON* scene_authoring = NULL;
        TEST_ASSERT(root != NULL);
        scene_authoring = cJSON_GetObjectItem(root, "sceneAuthoring");
        TEST_ASSERT(cJSON_IsObject(scene_authoring));
        TEST_ASSERT(cJSON_IsArray(cJSON_GetObjectItem(scene_authoring, "lights")));
        TEST_ASSERT(cJSON_IsArray(cJSON_GetObjectItem(scene_authoring, "cameras")));
        TEST_ASSERT(cJSON_IsArray(cJSON_GetObjectItem(scene_authoring, "paths")));
        {
            const cJSON* saved_path = cJSON_GetArrayItem(
                cJSON_GetObjectItem(scene_authoring, "paths"), (int)path_index);
            TEST_ASSERT(cJSON_IsArray(cJSON_GetObjectItem(saved_path, "tangentModes")));
        }
        TEST_ASSERT(cJSON_GetObjectItem(scene_authoring, "cameraPaths") == NULL);
        TEST_ASSERT(cJSON_IsArray(cJSON_GetObjectItem(scene_authoring, "materials")));
        cJSON_Delete(root);
    }

    Layout_SceneAuthoringState_Init(authoring);
    TEST_ASSERT(Layout_LoadFromString(layout, json));
    Layout_FreeString(json);
    authoring = &layout->sceneAuthoring;

    TEST_ASSERT(authoring->light_count == 2u);
    TEST_ASSERT(authoring->camera_count == 2u);
    TEST_ASSERT(authoring->path_count == 4u);
    TEST_ASSERT(authoring->material_count == 2u);
    TEST_ASSERT(strcmp(authoring->lights[1].light_id, "light_002") == 0);
    TEST_ASSERT(authoring->lights[1].kind == LINE_DRAWING_SCENE_LIGHT_SPOT);
    TEST_ASSERT(!authoring->lights[1].enabled);
    TEST_ASSERT(ld_test_vec3_nearly_equal(authoring->lights[1].position,
                                          (Vec3){ 1.0f, 2.0f, 3.0f }));
    TEST_ASSERT(ld_test_vec3_nearly_equal(authoring->lights[1].direction,
                                          (Vec3){ -0.25f, 0.5f, -1.0f }));
    TEST_ASSERT(ld_test_vec3_nearly_equal(authoring->lights[1].aim_target,
                                          (Vec3){ 5.0f, 6.0f, 7.0f }));
    TEST_ASSERT(authoring->lights[1].position_mode ==
                LINE_DRAWING_SCENE_LIGHT_POSITION_PATH_START);
    TEST_ASSERT(ld_test_nearly_equal(authoring->lights[1].color_rgb[1], 0.78f));
    TEST_ASSERT(ld_test_nearly_equal(authoring->lights[1].intensity, 5.0f));
    TEST_ASSERT(ld_test_nearly_equal(authoring->lights[1].radius, 1.0f));
    TEST_ASSERT(ld_test_nearly_equal(authoring->lights[1].area_size.x, 4.0f));
    TEST_ASSERT(ld_test_nearly_equal(authoring->lights[1].area_size.y, 8.0f));
    TEST_ASSERT(ld_test_nearly_equal(authoring->lights[1].inner_cone_degrees, 35.0f));
    TEST_ASSERT(ld_test_nearly_equal(authoring->lights[1].outer_cone_degrees, 60.0f));
    TEST_ASSERT(authoring->lights[1].falloff == LINE_DRAWING_SCENE_LIGHT_FALLOFF_LINEAR);
    TEST_ASSERT(strcmp(authoring->lights[1].path_id, "path_camera_003") == 0);
    TEST_ASSERT(authoring->paths[2].role == LINE_DRAWING_SCENE_PATH_ROLE_LIGHT);
    TEST_ASSERT(strcmp(authoring->paths[2].curve_type, "linear") == 0);
    TEST_ASSERT(strcmp(authoring->paths[2].bound_light_id, "light_002") == 0);
    TEST_ASSERT(ld_test_vec3_nearly_equal(authoring->paths[2].control_points[1],
                                          edited_point));
    TEST_ASSERT(authoring->paths[2].tangent_modes[0] ==
                LINE_DRAWING_SCENE_PATH_TANGENT_BROKEN);
    TEST_ASSERT(authoring->paths[3].role == LINE_DRAWING_SCENE_PATH_ROLE_GENERIC);
    TEST_ASSERT(authoring->cameras[1].orientation_mode ==
                LINE_DRAWING_SCENE_CAMERA_ORIENTATION_LOOK_AT_TARGET);
    TEST_ASSERT(ld_test_vec3_nearly_equal(authoring->cameras[1].look_at_target,
                                          (Vec3){4.0f, 5.0f, 6.0f}));
    TEST_ASSERT(ld_test_nearly_equal(authoring->cameras[1].roll_degrees, 15.0f));
    TEST_ASSERT(ld_test_nearly_equal(authoring->cameras[1].vertical_fov_degrees, 65.0f));
    TEST_ASSERT(ld_test_nearly_equal(authoring->cameras[1].near_clip, 0.5f));
    TEST_ASSERT(ld_test_nearly_equal(authoring->cameras[1].far_clip, 1000.0f));
    TEST_ASSERT(strcmp(authoring->materials[1].material_id, "mat_002") == 0);
    TEST_ASSERT(ld_test_nearly_equal(authoring->materials[1].rgba[0], 0.78f));
    TEST_ASSERT(ld_test_nearly_equal(authoring->materials[1].rgba[1], 0.50f));
    TEST_ASSERT(ld_test_nearly_equal(authoring->materials[1].rgba[2], 0.34f));
    TEST_ASSERT(authoring->selected_kind == LINE_DRAWING_SCENE_AUTHORING_SELECTION_MATERIAL);
    TEST_ASSERT(authoring->selected_index == 1u);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_json_missing_scene_authoring_keeps_seed_records(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    Layout* layout = &state->layout;

    const char* legacy_json =
        "{\"file\":{\"schemaVersion\":9,\"gridSize\":1},\"anchors\":[],\"walls\":[]}";
    TEST_ASSERT(Layout_LoadFromString(layout, legacy_json));
    TEST_ASSERT(layout->sceneAuthoring.light_count == 1u);
    TEST_ASSERT(strcmp(layout->sceneAuthoring.lights[0].light_id, "light_key") == 0);
    TEST_ASSERT(layout->sceneAuthoring.path_count == 2u);
    TEST_ASSERT(layout->sceneAuthoring.camera_count == 1u);
    TEST_ASSERT(strcmp(layout->sceneAuthoring.paths[0].path_id, "path_camera_main") == 0);
    TEST_ASSERT(layout->sceneAuthoring.material_count == 1u);
    TEST_ASSERT(strcmp(layout->sceneAuthoring.materials[0].material_id, "mat_default") == 0);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_layout_json_loads_legacy_camera_paths_and_repairs_bindings(void) {
    ld_test_init_runtime();
    Layout* layout = &Global_Get()->layout;
    const char* legacy_json =
        "{\"file\":{\"schemaVersion\":9,\"gridSize\":1},\"anchors\":[],\"walls\":[],"
        "\"sceneAuthoring\":{"
        "\"lights\":[{\"lightId\":\"legacy_light\",\"label\":\"Legacy Light\","
        "\"kind\":\"point\",\"position\":{\"x\":0,\"y\":0,\"z\":1},"
        "\"direction\":{\"x\":0,\"y\":0,\"z\":-1},"
        "\"pathId\":\"legacy_path\",\"enabled\":true}],"
        "\"cameraPaths\":[{\"pathId\":\"legacy_path\",\"label\":\"Legacy Path\","
        "\"curveType\":\"bezier\",\"boundLightId\":\"\",\"boundCameraId\":\"\","
        "\"controlPoints\":[{\"x\":0,\"y\":0,\"z\":0},{\"x\":1,\"y\":2,\"z\":3}]}],"
        "\"materials\":[],\"selection\":{\"kind\":\"camera_path\",\"index\":0}}}";

    TEST_ASSERT(Layout_LoadFromString(layout, legacy_json));
    TEST_ASSERT(layout->sceneAuthoring.path_count == 1u);
    TEST_ASSERT(layout->sceneAuthoring.paths[0].role == LINE_DRAWING_SCENE_PATH_ROLE_LIGHT);
    TEST_ASSERT(strcmp(layout->sceneAuthoring.paths[0].bound_light_id, "legacy_light") == 0);
    TEST_ASSERT(strcmp(layout->sceneAuthoring.lights[0].path_id, "legacy_path") == 0);
    TEST_ASSERT(layout->sceneAuthoring.selected_kind ==
                LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_scene_authoring_light_property_setters(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    LineDrawingSceneAuthoringState* authoring = &state->layout.sceneAuthoring;

    TEST_ASSERT(authoring->light_count == 1u);
    TEST_ASSERT(Layout_SceneAuthoringState_Select(authoring,
                                                  LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT,
                                                  0u));
    TEST_ASSERT(authoring->lights[0].enabled);
    TEST_ASSERT(authoring->lights[0].kind == LINE_DRAWING_SCENE_LIGHT_DIRECTIONAL);

    TEST_ASSERT(Layout_SceneAuthoringState_ToggleSelectedLightEnabled(authoring));
    TEST_ASSERT(!authoring->lights[0].enabled);
    TEST_ASSERT(Layout_SceneAuthoringState_CycleSelectedLightKind(authoring));
    TEST_ASSERT(authoring->lights[0].kind == LINE_DRAWING_SCENE_LIGHT_POINT);
    TEST_ASSERT(Layout_SceneAuthoringState_CycleSelectedLightKind(authoring));
    TEST_ASSERT(authoring->lights[0].kind == LINE_DRAWING_SCENE_LIGHT_SPOT);
    TEST_ASSERT(Layout_SceneAuthoringState_CycleSelectedLightKind(authoring));
    TEST_ASSERT(authoring->lights[0].kind == LINE_DRAWING_SCENE_LIGHT_AREA);
    TEST_ASSERT(Layout_SceneAuthoringState_CycleSelectedLightKind(authoring));
    TEST_ASSERT(authoring->lights[0].kind == LINE_DRAWING_SCENE_LIGHT_DIRECTIONAL);
    TEST_ASSERT(Layout_SceneAuthoringState_CycleSelectedLightPath(authoring));
    TEST_ASSERT(authoring->lights[0].path_id[0] == '\0');
    TEST_ASSERT(Layout_SceneAuthoringState_CycleSelectedLightPath(authoring));
    TEST_ASSERT(strcmp(authoring->lights[0].path_id, "path_light_key") == 0);
    TEST_ASSERT(strcmp(authoring->paths[1].bound_light_id, "light_key") == 0);

    TEST_ASSERT(Layout_SceneAuthoringState_Select(authoring,
                                                  LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH,
                                                  0u));
    TEST_ASSERT(Layout_SceneAuthoringState_CycleSelectedPathCurveType(authoring));
    TEST_ASSERT(strcmp(authoring->paths[0].curve_type, "linear") == 0);
    TEST_ASSERT(Layout_SceneAuthoringState_CycleSelectedPathCurveType(authoring));
    TEST_ASSERT(strcmp(authoring->paths[0].curve_type, "bezier") == 0);

    TEST_ASSERT(Layout_SceneAuthoringState_Select(authoring,
                                                  LINE_DRAWING_SCENE_AUTHORING_SELECTION_MATERIAL,
                                                  0u));
    TEST_ASSERT(Layout_SceneAuthoringState_CycleSelectedMaterialColor(authoring));
    TEST_ASSERT(ld_test_nearly_equal(authoring->materials[0].rgba[0], 0.45f));
    TEST_ASSERT(ld_test_nearly_equal(authoring->materials[0].rgba[1], 0.62f));
    TEST_ASSERT(ld_test_nearly_equal(authoring->materials[0].rgba[2], 0.80f));

    Vec3 moved_point = { 1.25f, 2.5f, 3.75f };
    TEST_ASSERT(Layout_SceneAuthoringState_SetPathControlPoint(authoring,
                                                                     0u,
                                                                     1u,
                                                                     moved_point));
    TEST_ASSERT(ld_test_vec3_nearly_equal(authoring->paths[0].control_points[1],
                                          moved_point));
    TEST_ASSERT(!Layout_SceneAuthoringState_SetPathControlPoint(authoring,
                                                                      authoring->path_count,
                                                                      0u,
                                                                      moved_point));
    TEST_ASSERT(!Layout_SceneAuthoringState_SetPathControlPoint(authoring,
                                                                      0u,
                                                                      authoring->paths[0].control_point_count,
                                                                      moved_point));
    TEST_ASSERT(Layout_SceneAuthoringState_SetLightPosition(authoring, 0u, moved_point));
    TEST_ASSERT(ld_test_vec3_nearly_equal(authoring->lights[0].position, moved_point));
    TEST_ASSERT(!Layout_SceneAuthoringState_SetLightPosition(authoring,
                                                            authoring->light_count,
                                                            moved_point));
    TEST_ASSERT(Layout_SceneAuthoringState_FindPathById(authoring,
                                                              "path_camera_main") ==
                &authoring->paths[0]);
    TEST_ASSERT(Layout_SceneAuthoringState_FindPathById(authoring, "missing") == NULL);

    Vec3 inserted_point = { 9.0f, 8.0f, 7.0f };
    TEST_ASSERT(Layout_SceneAuthoringState_InsertPathControlPoint(authoring,
                                                                        0u,
                                                                        1u,
                                                                        inserted_point));
    TEST_ASSERT(authoring->paths[0].control_point_count == 5u);
    TEST_ASSERT(ld_test_vec3_nearly_equal(authoring->paths[0].control_points[1],
                                          inserted_point));
    while (authoring->paths[0].control_point_count < LINE_DRAWING_SCENE_AUTHORING_MAX_PATH_POINTS) {
        TEST_ASSERT(Layout_SceneAuthoringState_InsertPathControlPoint(
            authoring,
            0u,
            authoring->paths[0].control_point_count,
            inserted_point));
    }
    TEST_ASSERT(!Layout_SceneAuthoringState_InsertPathControlPoint(authoring,
                                                                         0u,
                                                                         1u,
                                                                         inserted_point));
    TEST_ASSERT(Layout_SceneAuthoringState_DeletePathControlPoint(authoring, 0u, 1u));
    TEST_ASSERT(authoring->paths[0].control_point_count ==
                LINE_DRAWING_SCENE_AUTHORING_MAX_PATH_POINTS - 1u);
    TEST_ASSERT(!ld_test_vec3_nearly_equal(authoring->paths[0].control_points[1],
                                           inserted_point));
    while (authoring->paths[0].control_point_count > 2u) {
        TEST_ASSERT(Layout_SceneAuthoringState_DeletePathControlPoint(
            authoring, 0u, authoring->paths[0].control_point_count - 1u));
    }
    TEST_ASSERT(!Layout_SceneAuthoringState_DeletePathControlPoint(authoring, 0u, 1u));
    TEST_ASSERT(!Layout_SceneAuthoringState_DeletePathControlPoint(authoring,
                                                                         authoring->path_count,
                                                                         0u));

    Layout_SceneAuthoringState_ClearSelection(authoring);
    TEST_ASSERT(!Layout_SceneAuthoringState_ToggleSelectedLightEnabled(authoring));
    TEST_ASSERT(!Layout_SceneAuthoringState_CycleSelectedLightKind(authoring));
    TEST_ASSERT(!Layout_SceneAuthoringState_CycleSelectedLightPath(authoring));
    TEST_ASSERT(!Layout_SceneAuthoringState_CycleSelectedPathCurveType(authoring));
    TEST_ASSERT(!Layout_SceneAuthoringState_CycleSelectedMaterialColor(authoring));

    ld_test_shutdown_runtime();
    return true;
}

static bool test_scene_light_spatial_intent_and_presets(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    LineDrawingSceneAuthoringState* authoring = &state->layout.sceneAuthoring;
    LineDrawingSceneLight* light = &authoring->lights[0];
    LineDrawingScenePath* path = &authoring->paths[1];
    Vec3 independent = light->position;
    Vec3 target = {7.0f, 8.0f, 9.0f};
    TEST_ASSERT(Layout_SceneAuthoringState_Select(authoring,
                                                  LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT,
                                                  0u));
    TEST_ASSERT(ld_test_vec3_nearly_equal(
        Layout_SceneLight_EffectivePosition(light, path), independent));
    TEST_ASSERT(Layout_SceneAuthoringState_CycleSelectedLightPositionMode(authoring));
    TEST_ASSERT(light->position_mode == LINE_DRAWING_SCENE_LIGHT_POSITION_PATH_START);
    TEST_ASSERT(ld_test_vec3_nearly_equal(
        Layout_SceneLight_EffectivePosition(light, path), path->control_points[0]));
    TEST_ASSERT(Layout_SceneLight_SetAimPoint(light, path, target));
    TEST_ASSERT(ld_test_vec3_nearly_equal(Layout_SceneLight_AimPoint(light, path), target));
    TEST_ASSERT(ld_test_nearly_equal(Vec3_Length(
        Layout_SceneLight_EffectiveDirection(light, path)), 1.0f));
    TEST_ASSERT(Layout_SceneAuthoringState_CycleSelectedLightColor(authoring));
    TEST_ASSERT(ld_test_nearly_equal(light->color_rgb[0], 1.0f));
    TEST_ASSERT(ld_test_nearly_equal(light->color_rgb[1], 0.78f));
    TEST_ASSERT(Layout_SceneAuthoringState_CycleSelectedLightIntensity(authoring));
    TEST_ASSERT(ld_test_nearly_equal(light->intensity, 2.0f));
    TEST_ASSERT(Layout_SceneAuthoringState_CycleSelectedLightRadiusOrSize(authoring));
    TEST_ASSERT(ld_test_nearly_equal(light->radius, 0.5f));
    TEST_ASSERT(Layout_SceneAuthoringState_CycleSelectedLightCone(authoring));
    TEST_ASSERT(ld_test_nearly_equal(light->inner_cone_degrees, 35.0f));
    TEST_ASSERT(ld_test_nearly_equal(light->outer_cone_degrees, 60.0f));
    TEST_ASSERT(Layout_SceneAuthoringState_CycleSelectedLightFalloff(authoring));
    TEST_ASSERT(light->falloff == LINE_DRAWING_SCENE_LIGHT_FALLOFF_LINEAR);
    light->kind = LINE_DRAWING_SCENE_LIGHT_AREA;
    TEST_ASSERT(Layout_SceneAuthoringState_CycleSelectedLightRadiusOrSize(authoring));
    TEST_ASSERT(ld_test_nearly_equal(light->area_size.x, 4.0f));
    TEST_ASSERT(ld_test_nearly_equal(light->area_size.y, 4.0f));
    TEST_ASSERT(Layout_SceneAuthoringState_CycleSelectedLightPath(authoring));
    TEST_ASSERT(light->path_id[0] == '\0');
    TEST_ASSERT(light->position_mode == LINE_DRAWING_SCENE_LIGHT_POSITION_INDEPENDENT);
    TEST_ASSERT(ld_test_vec3_nearly_equal(
        Layout_SceneLight_EffectivePosition(light, NULL), independent));
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
        { "LayoutComputeCentroidIgnoresDeletedAnchors", test_layout_compute_centroid_ignores_deleted_anchors },
        { "LayoutSceneAuthoringDefaultsSeedAuthoringEntities",
          test_layout_scene_authoring_defaults_seed_authoring_entities },
        { "SceneAuthoringPathHandlesFollowSelectedRecords",
          test_scene_authoring_path_handles_follow_selected_records },
        { "SceneAuthoringPathHandlesPickSelectedCameraPathPoint",
          test_scene_authoring_path_handles_pick_selected_camera_path_point },
        { "SceneAuthoringPathGizmoPickStaysScreenSizedAcrossZoom",
          test_scene_authoring_path_gizmo_pick_stays_screen_sized_across_zoom },
        { "SceneAuthoringAxisDragsIsolateCameraAndLightCoordinates",
          test_scene_authoring_axis_drags_isolate_camera_and_light_coordinates },
        { "SceneAuthoringCenterDragUsesConstructionPlane",
          test_scene_authoring_center_drag_uses_construction_plane },
        { "SceneAuthoringGizmoPickPriorityAndDegenerateAxisPolicy",
          test_scene_authoring_gizmo_pick_priority_and_degenerate_axis_policy },
        { "ScenePathGeometryEvaluatesLinearAndCubicContracts",
          test_scene_path_geometry_evaluates_linear_and_cubic_contracts },
        { "ScenePathGeometryHandlesContinuityFallbackAndDegeneracy",
          test_scene_path_geometry_handles_continuity_fallback_and_degeneracy },
        { "ScenePathGeometryCubicSplitPreservesCurve",
          test_scene_path_geometry_cubic_split_preserves_curve },
        { "ScenePathTraversalUsesWorldDistanceAndLoopPolicy",
          test_scene_path_traversal_uses_world_distance_and_loop_policy },
        { "ScenePathTraversalDrivesCameraAndLightFollowers",
          test_scene_path_traversal_drives_camera_and_light_followers },
        { "ScenePathEditClassifiesTypedElementsAndModes",
          test_scene_path_edit_classifies_typed_elements_and_modes },
        { "ScenePathEditSplitAndSafeDeletePreserveContract",
          test_scene_path_edit_split_and_safe_delete_preserve_contract },
        { "SceneAuthoringTypedTangentPickSurvivesDragSession",
          test_scene_authoring_typed_tangent_pick_survives_drag_session },
        { "SceneCameraRecordsModesPoseAndDelete",
          test_scene_camera_records_modes_pose_and_delete },
        { "SceneCameraAimPickSurvivesAxisDrag",
          test_scene_camera_aim_pick_survives_axis_drag },
        { "SceneLightAimPickSurvivesAxisDrag",
          test_scene_light_aim_pick_survives_axis_drag },
        { "SceneAuthoringBezierScreenInsertionUsesSampledGeometry",
          test_scene_authoring_bezier_screen_insertion_uses_sampled_geometry },
        { "SceneAuthoringDeletesSelectedRecordsAndClearsReferences",
          test_scene_authoring_deletes_selected_records_and_clears_references },
        { "LayoutJsonPersistsSceneAuthoringRecords",
          test_layout_json_persists_scene_authoring_records },
        { "LayoutJsonMissingSceneAuthoringKeepsSeedRecords",
          test_layout_json_missing_scene_authoring_keeps_seed_records },
        { "LayoutJsonLoadsLegacyCameraPathsAndRepairsBindings",
          test_layout_json_loads_legacy_camera_paths_and_repairs_bindings },
        { "SceneAuthoringLightPropertySetters",
          test_scene_authoring_light_property_setters },
        { "SceneLightSpatialIntentAndPresets",
          test_scene_light_spatial_intent_and_presets }
    };

    return run_test_cases("LayoutCore", cases, sizeof(cases) / sizeof(cases[0]));
}
