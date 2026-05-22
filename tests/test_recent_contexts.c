#include "test_framework.h"

#include "Core/recent_contexts.h"
#include "Menu/line_drawing_recent_contexts.h"

#include <string.h>

static bool test_recent_contexts_deduplicate_and_promote_paths(void) {
    LineDrawingRecentContexts contexts;
    int i = 0;

    LineDrawingRecentContexts_Init(&contexts);
    TEST_ASSERT(LineDrawingRecentContexts_TrackLayout(&contexts, "config/a.json"));
    TEST_ASSERT(LineDrawingRecentContexts_TrackLayout(&contexts, "config/b.json"));
    TEST_ASSERT(LineDrawingRecentContexts_TrackLayout(&contexts, "config/a.json"));
    TEST_ASSERT(contexts.layouts.count == 2);
    TEST_ASSERT(strcmp(contexts.layouts.paths[0], "config/a.json") == 0);
    TEST_ASSERT(strcmp(contexts.layouts.paths[1], "config/b.json") == 0);

    for (i = 0; i < LINE_DRAWING_RECENT_CONTEXT_LIMIT + 2; ++i) {
        char path[64];
        snprintf(path, sizeof(path), "export/run_%d", i);
        TEST_ASSERT(LineDrawingRecentContexts_TrackOutputRoot(&contexts, path));
    }
    TEST_ASSERT(contexts.output_roots.count == LINE_DRAWING_RECENT_CONTEXT_LIMIT);
    TEST_ASSERT(strcmp(contexts.output_roots.paths[0], "export/run_9") == 0);
    TEST_ASSERT(strcmp(contexts.output_roots.paths[LINE_DRAWING_RECENT_CONTEXT_LIMIT - 1],
                       "export/run_2") == 0);
    return true;
}

static bool test_recent_menu_list_groups_layouts_scenes_and_roots(void) {
    LineDrawingRecentContexts contexts;
    LineDrawingRecentMenuList list;

    LineDrawingRecentContexts_Init(&contexts);
    TEST_ASSERT(LineDrawingRecentContexts_TrackLayout(&contexts, "/tmp/layouts/test.layout.json"));
    TEST_ASSERT(LineDrawingRecentContexts_TrackScene(&contexts, "/tmp/scenes/test_scene"));
    TEST_ASSERT(LineDrawingRecentContexts_TrackInputRoot(&contexts, "/tmp/input_root"));
    TEST_ASSERT(LineDrawingRecentContexts_TrackOutputRoot(&contexts, "/tmp/output_root"));

    LineDrawingRecentMenuList_Refresh(&list,
                                      &contexts,
                                      "/tmp/layouts/test.layout.json",
                                      "/tmp/scenes/test_scene",
                                      "/tmp/input_root",
                                      "/tmp/output_root");

    TEST_ASSERT(list.entry_count == 4);
    TEST_ASSERT(list.layout_count == 1);
    TEST_ASSERT(list.scene_count == 1);
    TEST_ASSERT(list.input_root_count == 1);
    TEST_ASSERT(list.output_root_count == 1);
    TEST_ASSERT(list.entries[0].kind == LINE_DRAWING_RECENT_MENU_ENTRY_LAYOUT);
    TEST_ASSERT(list.entries[0].current);
    TEST_ASSERT(strcmp(list.entries[0].label, "test.layout.json") == 0);
    TEST_ASSERT(list.entries[2].kind == LINE_DRAWING_RECENT_MENU_ENTRY_INPUT_ROOT);
    TEST_ASSERT(strcmp(list.entries[2].label, "input_root") == 0);
    TEST_ASSERT(list.entries[3].kind == LINE_DRAWING_RECENT_MENU_ENTRY_OUTPUT_ROOT);
    TEST_ASSERT(strcmp(list.entries[3].label, "output_root") == 0);
    return true;
}

bool recent_contexts_run_tests(void) {
    const TestCase cases[] = {
        {"DeduplicateAndPromotePaths", test_recent_contexts_deduplicate_and_promote_paths},
        {"MenuListGroupsLayoutsScenesAndRoots", test_recent_menu_list_groups_layouts_scenes_and_roots},
    };
    return run_test_cases("RecentContexts", cases, sizeof(cases) / sizeof(cases[0]));
}
