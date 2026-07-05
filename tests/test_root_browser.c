#include "test_framework.h"

#include "Menu/line_drawing_root_browser.h"
#include "test_artifact_helpers.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static bool test_root_browser_builds_picker_actions_and_nearby_suggestions(void) {
    char temp_template[] = "/tmp/ld_root_browser_nearby_XXXXXX";
    char* root = NULL;
    char family_dir[PATH_MAX];
    char current_dir[PATH_MAX];
    char sibling_dir[PATH_MAX];
    char cousin_branch_dir[PATH_MAX];
    char cousin_dir[PATH_MAX];
    char sibling_scene_file[PATH_MAX];
    char cousin_json_file[PATH_MAX];
    LineDrawingRootBrowser browser;

    root = ld_test_artifact_make_temp_dir(temp_template);
    TEST_ASSERT(root != NULL);
    snprintf(family_dir, sizeof(family_dir), "%s/family", root);
    snprintf(current_dir, sizeof(current_dir), "%s/family/current_room", root);
    snprintf(sibling_dir, sizeof(sibling_dir), "%s/family/gallery_room", root);
    snprintf(cousin_branch_dir, sizeof(cousin_branch_dir), "%s/other_branch", root);
    snprintf(cousin_dir, sizeof(cousin_dir), "%s/other_branch/scene_sets", root);
    TEST_ASSERT(ld_test_artifact_scene_authoring_path(sibling_scene_file,
                                                      sizeof(sibling_scene_file),
                                                      sibling_dir));
    snprintf(cousin_json_file, sizeof(cousin_json_file), "%s/request.json", cousin_dir);

    TEST_ASSERT(ld_test_artifact_make_dir(family_dir));
    TEST_ASSERT(ld_test_artifact_make_dir(current_dir));
    TEST_ASSERT(ld_test_artifact_make_dir(sibling_dir));
    TEST_ASSERT(ld_test_artifact_make_dir(cousin_branch_dir));
    TEST_ASSERT(ld_test_artifact_make_dir(cousin_dir));
    TEST_ASSERT(ld_test_artifact_write_text_file(sibling_scene_file, "{}"));
    TEST_ASSERT(ld_test_artifact_write_text_file(cousin_json_file, "{}"));

    LineDrawingRootBrowser_Refresh(&browser, current_dir, current_dir, "export");
    TEST_ASSERT(browser.entry_count >= 2);
    TEST_ASSERT(browser.nearby_count >= 2);
    TEST_ASSERT(browser.entries[0].kind == LINE_DRAWING_ROOT_BROWSER_ENTRY_NEARBY_INPUT_ROOT);
    TEST_ASSERT(strcmp(browser.entries[0].label, "gallery_room") == 0);
    TEST_ASSERT(browser.entries[0].preview_kind == LINE_DRAWING_CATALOG_PREVIEW_SOURCE_SCENE);
    TEST_ASSERT(strcmp(browser.entries[0].preview_path, sibling_scene_file) == 0);
    TEST_ASSERT(strcmp(browser.entries[1].label, "other_branch/scene_sets") == 0);
    TEST_ASSERT(browser.entries[1].preview_kind == LINE_DRAWING_CATALOG_PREVIEW_SOURCE_LAYOUT);
    TEST_ASSERT(strcmp(browser.entries[1].preview_path, cousin_json_file) == 0);
    return true;
}

static bool test_root_browser_uses_input_root_as_anchor_when_browse_root_is_missing(void) {
    char temp_template[] = "/tmp/ld_root_browser_input_XXXXXX";
    char* root = NULL;
    char sibling_dir[PATH_MAX];
    char sibling_json[PATH_MAX];
    bool found_scenes = false;
    int i = 0;
    LineDrawingRootBrowser browser;

    root = ld_test_artifact_make_temp_dir(temp_template);
    TEST_ASSERT(root != NULL);
    snprintf(sibling_dir, sizeof(sibling_dir), "%s/scenes", root);
    snprintf(sibling_json, sizeof(sibling_json), "%s/layout.json", sibling_dir);
    TEST_ASSERT(ld_test_artifact_make_dir(sibling_dir));
    TEST_ASSERT(ld_test_artifact_write_text_file(sibling_json, "{}"));

    LineDrawingRootBrowser_Refresh(&browser, NULL, root, "export");
    TEST_ASSERT(strcmp(browser.current_path, root) == 0);
    TEST_ASSERT(browser.nearby_count >= 1);
    for (i = 0; i < browser.entry_count; ++i) {
        if (browser.entries[i].kind == LINE_DRAWING_ROOT_BROWSER_ENTRY_NEARBY_INPUT_ROOT &&
            strcmp(browser.entries[i].label, "scenes") == 0) {
            found_scenes = true;
            TEST_ASSERT(browser.entries[i].preview_kind == LINE_DRAWING_CATALOG_PREVIEW_SOURCE_LAYOUT);
            TEST_ASSERT(strcmp(browser.entries[i].preview_path, sibling_json) == 0);
            break;
        }
    }
    TEST_ASSERT(found_scenes);
    return true;
}

bool root_browser_run_tests(void) {
    const TestCase cases[] = {
        {"BuildsPickerActionsAndNearbySuggestions",
         test_root_browser_builds_picker_actions_and_nearby_suggestions},
        {"UsesInputRootAsAnchorWhenBrowseRootIsMissing",
         test_root_browser_uses_input_root_as_anchor_when_browse_root_is_missing},
    };
    return run_test_cases("RootBrowser", cases, sizeof(cases) / sizeof(cases[0]));
}
