#include "test_framework.h"

#include "Core/global_state.h"
#include "Layout/layout.h"
#include "Layout/layout_json.h"
#include "Layout/hitbox_system.h"
#include "Editor/editor.h"
#include "Math/math_util.h"
#include "Tools/shape_from_layout.h"
#include "cjson/cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static bool nearly_equal(float a, float b) {
    return fabsf(a - b) < 0.0001f;
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
        { "LayoutComputeCentroidIgnoresDeletedAnchors", test_layout_compute_centroid_ignores_deleted_anchors },
        { "ShapeExportProjectionAxisMapping", test_shape_export_projection_axis_mapping }
    };

    return run_test_cases("Layout", cases, sizeof(cases) / sizeof(cases[0]));
}
