#include "test_framework.h"

#include "Menu/line_drawing_host_menu.h"
#include "test_artifact_helpers.h"
#include "test_layout_internal.h"

#include <SDL2/SDL.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static bool test_first_selectable_prefers_resume(void) {
    LineDrawingHostMenuModel model = {0};
    model.item_enabled[LINE_DRAWING_HOST_MENU_ITEM_RESUME_EDITOR] = true;
    model.item_enabled[LINE_DRAWING_HOST_MENU_ITEM_QUIT] = true;
    TEST_ASSERT(LineDrawingHostMenu_FirstSelectableIndex(&model) ==
                LINE_DRAWING_HOST_MENU_ITEM_RESUME_EDITOR);
    return true;
}

static bool test_move_selection_skips_disabled_items(void) {
    LineDrawingHostMenuModel model = {0};
    model.item_enabled[LINE_DRAWING_HOST_MENU_ITEM_RESUME_EDITOR] = true;
    model.item_enabled[LINE_DRAWING_HOST_MENU_ITEM_LOAD_LAST_LAYOUT] = false;
    model.item_enabled[LINE_DRAWING_HOST_MENU_ITEM_LOAD_LAST_SCENE] = false;
    model.item_enabled[LINE_DRAWING_HOST_MENU_ITEM_QUIT] = true;

    TEST_ASSERT(LineDrawingHostMenu_MoveSelection(&model,
                                                  LINE_DRAWING_HOST_MENU_ITEM_RESUME_EDITOR,
                                                  1) == LINE_DRAWING_HOST_MENU_ITEM_QUIT);
    TEST_ASSERT(LineDrawingHostMenu_MoveSelection(&model,
                                                  LINE_DRAWING_HOST_MENU_ITEM_QUIT,
                                                  -1) == LINE_DRAWING_HOST_MENU_ITEM_RESUME_EDITOR);
    return true;
}

static bool test_invalid_current_selection_falls_back_to_first_enabled(void) {
    LineDrawingHostMenuModel model = {0};
    model.item_enabled[LINE_DRAWING_HOST_MENU_ITEM_LOAD_LAST_LAYOUT] = true;
    model.item_enabled[LINE_DRAWING_HOST_MENU_ITEM_QUIT] = true;

    TEST_ASSERT(LineDrawingHostMenu_MoveSelection(&model, -1, 1) ==
                LINE_DRAWING_HOST_MENU_ITEM_LOAD_LAST_LAYOUT);
    TEST_ASSERT(LineDrawingHostMenu_MoveSelection(&model, 99, -1) ==
                LINE_DRAWING_HOST_MENU_ITEM_LOAD_LAST_LAYOUT);
    return true;
}

static bool test_mouse_hover_does_not_commit_nav_selection(void) {
    LineDrawingHostMenuState state;
    LineDrawingHostMenuCommand command = {0};
    AppContext ctx = {0};
    SDL_Event event;

    ld_test_init_runtime();
    LineDrawingHostMenu_Init(&state);
    state.selected_section = LINE_DRAWING_HOST_MENU_SECTION_QUICK_ACTIONS;

    memset(&event, 0, sizeof(event));
    event.type = SDL_MOUSEMOTION;
    event.motion.x = 48;
    event.motion.y = 360;

    TEST_ASSERT(LineDrawingHostMenu_HandleEvent(&state, &ctx, &event, &command));
    TEST_ASSERT(state.selected_section == LINE_DRAWING_HOST_MENU_SECTION_QUICK_ACTIONS);
    TEST_ASSERT(state.hovered_section_index == LINE_DRAWING_HOST_MENU_SECTION_BROWSE);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_mouse_hover_does_not_commit_content_selection(void) {
    LineDrawingHostMenuState state;
    LineDrawingHostMenuCommand command = {0};
    AppContext ctx = {0};
    SDL_Event event;

    ld_test_init_runtime();
    LineDrawingHostMenu_Init(&state);
    state.selected_section = LINE_DRAWING_HOST_MENU_SECTION_QUICK_ACTIONS;
    state.selected_index = LINE_DRAWING_HOST_MENU_ITEM_QUIT;

    memset(&event, 0, sizeof(event));
    event.type = SDL_MOUSEMOTION;
    event.motion.x = 260;
    event.motion.y = 248;

    TEST_ASSERT(LineDrawingHostMenu_HandleEvent(&state, &ctx, &event, &command));
    TEST_ASSERT(state.selected_index == LINE_DRAWING_HOST_MENU_ITEM_QUIT);
    TEST_ASSERT(state.hovered_content_index == LINE_DRAWING_HOST_MENU_ITEM_RESUME_EDITOR);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_mouse_click_commits_nav_selection(void) {
    LineDrawingHostMenuState state;
    LineDrawingHostMenuCommand command = {0};
    AppContext ctx = {0};
    SDL_Event event;

    ld_test_init_runtime();
    LineDrawingHostMenu_Init(&state);
    state.selected_section = LINE_DRAWING_HOST_MENU_SECTION_QUICK_ACTIONS;

    memset(&event, 0, sizeof(event));
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = 48;
    event.button.y = 360;

    TEST_ASSERT(LineDrawingHostMenu_HandleEvent(&state, &ctx, &event, &command));
    TEST_ASSERT(state.selected_section == LINE_DRAWING_HOST_MENU_SECTION_BROWSE);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_browse_click_switches_to_catalog_section(void) {
    char temp_template[] = "/tmp/ld_host_menu_browse_click_XXXXXX";
    char* root = NULL;
    char family_dir[PATH_MAX];
    char current_dir[PATH_MAX];
    char sibling_dir[PATH_MAX];
    char sibling_scene_file[PATH_MAX];
    char sibling_runtime_file[PATH_MAX];
    LineDrawingHostMenuState state;
    LineDrawingHostMenuCommand command = {0};
    AppContext ctx = {0};
    SDL_Event event;
    char prior_input_root[LINE_DRAWING_PATH_CAP];
    LineDrawingRecentContexts prior_recents;

    root = mkdtemp(temp_template);
    TEST_ASSERT(root != NULL);
    snprintf(family_dir, sizeof(family_dir), "%s/family", root);
    snprintf(current_dir, sizeof(current_dir), "%s/family/current_room", root);
    snprintf(sibling_dir, sizeof(sibling_dir), "%s/family/gallery_room", root);
    TEST_ASSERT(ld_test_artifact_scene_authoring_path(sibling_scene_file,
                                                      sizeof(sibling_scene_file),
                                                      sibling_dir));
    TEST_ASSERT(ld_test_artifact_scene_runtime_path(sibling_runtime_file,
                                                    sizeof(sibling_runtime_file),
                                                    sibling_dir));

    TEST_ASSERT(ld_test_artifact_make_dir(family_dir));
    TEST_ASSERT(ld_test_artifact_make_dir(current_dir));
    TEST_ASSERT(ld_test_artifact_make_dir(sibling_dir));
    TEST_ASSERT(ld_test_artifact_write_text_file(sibling_scene_file, "{}"));
    TEST_ASSERT(ld_test_artifact_write_text_file(sibling_runtime_file, "{}"));

    ld_test_init_runtime();
    snprintf(prior_input_root,
             sizeof(prior_input_root),
             "%s",
             Global_GetInputRoot());
    prior_recents = *Global_GetRecentContexts();
    TEST_ASSERT(Global_SetInputRoot(current_dir, true));

    LineDrawingHostMenu_Init(&state);
    state.selected_section = LINE_DRAWING_HOST_MENU_SECTION_BROWSE;
    state.selected_browser_index = 0;

    memset(&event, 0, sizeof(event));
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = 260;
    event.button.y = 248;

    TEST_ASSERT(LineDrawingHostMenu_HandleEvent(&state, &ctx, &event, &command));
    TEST_ASSERT(strcmp(Global_GetInputRoot(), sibling_dir) == 0);
    TEST_ASSERT(state.selected_section == LINE_DRAWING_HOST_MENU_SECTION_SCENES ||
                state.selected_section == LINE_DRAWING_HOST_MENU_SECTION_LAYOUTS);
    TEST_ASSERT(state.focus_region == LINE_DRAWING_HOST_MENU_FOCUS_CONTENT);

    TEST_ASSERT(Global_SetInputRoot(prior_input_root, true));
    TEST_ASSERT(LineDrawingRecentContexts_Save(&prior_recents));
    ld_test_shutdown_runtime();
    TEST_ASSERT(unlink(sibling_scene_file) == 0);
    TEST_ASSERT(unlink(sibling_runtime_file) == 0);
    TEST_ASSERT(rmdir(sibling_dir) == 0);
    TEST_ASSERT(rmdir(current_dir) == 0);
    TEST_ASSERT(rmdir(family_dir) == 0);
    TEST_ASSERT(rmdir(root) == 0);
    return true;
}

static bool test_last_layout_and_scene_paths_remain_independent(void) {
    const char* layout_path = "/private/tmp/line_drawing_last_layout.json";
    const char* scene_path = "/private/tmp/line_drawing_last_scene/scene_authoring.json";
    const char* scene_layout_hint = "/private/tmp/line_drawing_last_scene/layout_from_scene.json";

    ld_test_artifact_clear_recent_context_files();
    ld_test_init_runtime();

    Global_OnLayoutLoaded(layout_path);
    TEST_ASSERT(strcmp(Global_GetCurrentConfigPath(), layout_path) == 0);
    TEST_ASSERT(strcmp(Global_GetLastLayoutPath(), layout_path) == 0);
    TEST_ASSERT(Global_GetCurrentSceneAuthoringPath()[0] == '\0');
    TEST_ASSERT(Global_GetLastSceneAuthoringPath()[0] == '\0');

    Global_OnSceneLoaded(scene_path, scene_layout_hint);
    TEST_ASSERT(strcmp(Global_GetCurrentConfigPath(), scene_layout_hint) == 0);
    TEST_ASSERT(strcmp(Global_GetCurrentSceneAuthoringPath(), scene_path) == 0);
    TEST_ASSERT(strcmp(Global_GetLastLayoutPath(), layout_path) == 0);
    TEST_ASSERT(strcmp(Global_GetLastSceneAuthoringPath(), scene_path) == 0);

    ld_test_shutdown_runtime();
    ld_test_artifact_clear_recent_context_files();
    return true;
}

bool host_menu_run_tests(void) {
    const TestCase cases[] = {
        {"FirstSelectablePrefersResume", test_first_selectable_prefers_resume},
        {"MoveSelectionSkipsDisabledItems", test_move_selection_skips_disabled_items},
        {"InvalidSelectionFallsBack", test_invalid_current_selection_falls_back_to_first_enabled},
        {"MouseHoverDoesNotCommitNavSelection", test_mouse_hover_does_not_commit_nav_selection},
        {"MouseHoverDoesNotCommitContentSelection", test_mouse_hover_does_not_commit_content_selection},
        {"MouseClickCommitsNavSelection", test_mouse_click_commits_nav_selection},
        {"BrowseClickSwitchesToCatalogSection", test_browse_click_switches_to_catalog_section},
        {"LastLayoutAndScenePathsRemainIndependent",
         test_last_layout_and_scene_paths_remain_independent},
    };
    return run_test_cases("HostMenu", cases, sizeof(cases) / sizeof(cases[0]));
}
