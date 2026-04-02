#include "test_framework.h"

#include "Core/global_state.h"
#include "Layout/layout.h"
#include "Layout/layout_json.h"
#include "Layout/hitbox_system.h"
#include "Editor/editor.h"
#include "Math/math_util.h"
#include "Tools/canonical_scene_export.h"
#include "Tools/shape_from_layout.h"
#include "cjson/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static bool nearly_equal(float a, float b) {
    return fabsf(a - b) < 0.0001f;
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
        cJSON* space_mode_default = cJSON_GetObjectItem(root, "space_mode_default");
        cJSON* unit_system = cJSON_GetObjectItem(root, "unit_system");
        TEST_ASSERT(cJSON_IsString(schema_family));
        TEST_ASSERT(cJSON_IsString(schema_variant));
        TEST_ASSERT(cJSON_IsString(scene_id));
        TEST_ASSERT(cJSON_IsString(space_mode_default));
        TEST_ASSERT(cJSON_IsString(unit_system));
        TEST_ASSERT(strcmp(schema_family->valuestring, "codework_scene") == 0);
        TEST_ASSERT(strcmp(schema_variant->valuestring, "scene_authoring_v1") == 0);
        TEST_ASSERT(strcmp(scene_id->valuestring, "scene_test_2d") == 0);
        TEST_ASSERT(strcmp(space_mode_default->valuestring, "2d") == 0);
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
        cJSON* object_id = cJSON_GetObjectItem(obj, "object_id");
        cJSON* dimensional_mode = cJSON_GetObjectItem(obj, "dimensional_mode");
        cJSON* locked_plane = cJSON_GetObjectItem(obj, "locked_plane");
        cJSON* material_ref = cJSON_GetObjectItem(obj, "material_ref");
        TEST_ASSERT(cJSON_IsString(object_id));
        TEST_ASSERT(cJSON_IsString(dimensional_mode));
        TEST_ASSERT(cJSON_IsString(locked_plane));
        TEST_ASSERT(cJSON_IsObject(material_ref));
        TEST_ASSERT(strcmp(cJSON_GetObjectItem(material_ref, "id")->valuestring, "mat_line_drawing_default") == 0);
        TEST_ASSERT(strcmp(object_id->valuestring, "obj_line_drawing_layout") == 0);
        TEST_ASSERT(strcmp(dimensional_mode->valuestring, "plane_locked") == 0);
        TEST_ASSERT(strcmp(locked_plane->valuestring, "xy") == 0);
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
        cJSON* space_mode_default = cJSON_GetObjectItem(root, "space_mode_default");
        TEST_ASSERT(cJSON_IsString(space_mode_default));
        TEST_ASSERT(strcmp(space_mode_default->valuestring, "3d") == 0);
    }

    cJSON* objects = cJSON_GetObjectItem(root, "objects");
    TEST_ASSERT(cJSON_IsArray(objects));
    TEST_ASSERT(cJSON_GetArraySize(objects) == 3);

    cJSON* obj = find_object_by_id(objects, "obj_line_drawing_layout");
    TEST_ASSERT(cJSON_IsObject(obj));
    {
        cJSON* dimensional_mode = cJSON_GetObjectItem(obj, "dimensional_mode");
        TEST_ASSERT(cJSON_IsString(dimensional_mode));
        TEST_ASSERT(strcmp(dimensional_mode->valuestring, "full_3d") == 0);
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

bool layout_run_tests(void) {
    const TestCase cases[] = {
        { "AddWallReusesAnchors", test_layout_add_wall_reuses_anchors },
        { "RemoveWallCompactsOnDemand", test_layout_remove_wall_compacts_on_demand },
        { "LayoutStringRoundtrip", test_layout_string_roundtrip },
        { "EditorUndoRedoRestoresLayout", test_editor_undo_redo_restores_layout },
        { "LayoutJsonEmbedsVersion", test_layout_json_embeds_version },
        { "LayoutJsonFutureVersionRejected", test_layout_json_future_version_rejected },
        { "LayoutJsonMissingVersionDefaults", test_layout_json_missing_version_defaults },
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
        { "LayoutComputeCentroidIgnoresDeletedAnchors", test_layout_compute_centroid_ignores_deleted_anchors },
        { "ShapeExportProjectionAxisMapping", test_shape_export_projection_axis_mapping },
        { "CanonicalSceneExport2DPayload", test_canonical_scene_export_2d_payload },
        { "CanonicalSceneExport3DPayload", test_canonical_scene_export_3d_payload },
        { "CanonicalSceneExportPreservesExistingExtensions", test_canonical_scene_export_preserves_existing_extensions }
    };

    return run_test_cases("Layout", cases, sizeof(cases) / sizeof(cases[0]));
}
