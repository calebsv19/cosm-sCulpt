#include "test_framework.h"

#include "Core/global_state.h"
#include "Layout/layout.h"
#include "Layout/layout_json.h"
#include "Editor/editor.h"
#include "Math/math_util.h"
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
    TEST_ASSERT(nearly_equal(loaded->handleInLength, 2.5f));
    TEST_ASSERT(nearly_equal(loaded->handleInAngleDeg, 45.0f));
    TEST_ASSERT(nearly_equal(loaded->handleOutLength, 1.25f));
    TEST_ASSERT(nearly_equal(loaded->handleOutAngleDeg, -60.0f));

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
        { "LayoutHandlesLinkToggle", test_layout_handles_link_toggle },
        { "EditorHistoryLimitEnforced", test_editor_history_limit_enforced }
    };

    return run_test_cases("Layout", cases, sizeof(cases) / sizeof(cases[0]));
}
