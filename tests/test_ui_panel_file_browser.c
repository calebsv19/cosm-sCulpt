#include "test_layout_internal.h"

#include "UI/ui_panel.h"
#include "UI/ui_panel_internal.h"
#include "UI/ui_panel_file_layout.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static void ld_test_remove_file_browser_runtime_state(void) {
    (void)unlink("data/runtime/file_browser_mode.txt");
    (void)unlink("data/runtime/file_browser_json_root.txt");
    (void)unlink("data/runtime/file_browser_scene_root.txt");
    (void)unlink("data/runtime/file_browser_last_json_entry.txt");
    (void)unlink("data/runtime/file_browser_last_scene_entry.txt");
    (void)unlink("data/runtime/recent_layouts.txt");
    (void)unlink("data/runtime/recent_scenes.txt");
    (void)unlink("data/runtime/recent_input_roots.txt");
    (void)unlink("data/runtime/recent_output_roots.txt");
    (void)unlink("data/runtime/input_root.txt");
    (void)unlink("data/runtime/output_root.txt");
    (void)unlink("data/runtime/layout_root.txt");
}

static bool ld_test_write_layout_json(const char* path) {
    GlobalState* state = Global_Get();
    if (!state || !path || !path[0]) return false;
    return Layout_SaveToFile(&state->layout, path);
}

static int ld_test_find_load_menu_index(const UIPanelState* ui, const char* label) {
    if (!ui || !label) return -1;
    for (int i = 0; i < ui->loadMenu.count; ++i) {
        if (strcmp(ui->loadMenu.entries[i], label) == 0) return i;
    }
    return -1;
}

static bool ld_test_write_scene_contract(const char* scene_dir) {
    char authoring_path[LINE_DRAWING_PATH_CAP];
    char runtime_path[LINE_DRAWING_PATH_CAP];
    FILE* file = NULL;
    if (!scene_dir || !scene_dir[0]) return false;
    if (mkdir(scene_dir, 0755) != 0 && errno != EEXIST) return false;
    snprintf(authoring_path, sizeof(authoring_path), "%s/scene_authoring.json", scene_dir);
    snprintf(runtime_path, sizeof(runtime_path), "%s/scene_runtime.json", scene_dir);
    file = fopen(authoring_path, "wb");
    if (!file) return false;
    fputs("{\"schema\":\"scene_authoring_v1\"}\n", file);
    fclose(file);
    file = fopen(runtime_path, "wb");
    if (!file) return false;
    fputs("{\"schema\":\"scene_runtime_v1\"}\n", file);
    fclose(file);
    return true;
}

static bool test_file_browser_switches_modes_and_uses_pane_rect(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    char temp_root[LINE_DRAWING_PATH_CAP];
    char alpha_path[LINE_DRAWING_PATH_CAP];
    char beta_path[LINE_DRAWING_PATH_CAP];
    char scene_dir[LINE_DRAWING_PATH_CAP];
    SDL_Rect browser_rect = {0};

    snprintf(temp_root, sizeof(temp_root), "/tmp/ld_file_browser_%u", (unsigned)SDL_GetTicks());
    TEST_ASSERT(mkdir(temp_root, 0755) == 0 || errno == EEXIST);
    ld_test_remove_file_browser_runtime_state();
    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);

    snprintf(alpha_path, sizeof(alpha_path), "%s/alpha.json", temp_root);
    snprintf(beta_path, sizeof(beta_path), "%s/beta.json", temp_root);
    snprintf(scene_dir, sizeof(scene_dir), "%s/scene_one", temp_root);
    TEST_ASSERT(ld_test_write_layout_json(alpha_path));
    TEST_ASSERT(ld_test_write_layout_json(beta_path));
    TEST_ASSERT(ld_test_write_scene_contract(scene_dir));
    TEST_ASSERT(Global_SetInputRoot(temp_root, false));

    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    ui->activeLeftTab = UI_PANEL_LEFT_TAB_FILE;
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);

    UIPanel_ActivateJsonBrowser();
    TEST_ASSERT(UIPanel_IsLoadMenuOpen());
    TEST_ASSERT(ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON);
    TEST_ASSERT(ui->loadMenu.count == 2);
    TEST_ASSERT(UIPanel_GetFilePaneRects(ui, NULL, NULL, NULL, &browser_rect));
    TEST_ASSERT(browser_rect.h > 40);
    TEST_ASSERT(browser_rect.y >= ui->filePane.summaryRect.y + ui->filePane.summaryRect.h);
    TEST_ASSERT(browser_rect.y + browser_rect.h <= ui->filePane.fileActionsRect.y + 1);

    UIPanel_ActivateSceneBrowser();
    TEST_ASSERT(UIPanel_IsLoadMenuOpen());
    TEST_ASSERT(ui->loadMenu.mode == UI_LOAD_MENU_MODE_SCENE);
    TEST_ASSERT(ui->loadMenu.count == 1);

    ld_test_shutdown_runtime();
    ld_test_remove_file_browser_runtime_state();
    (void)unlink(alpha_path);
    (void)unlink(beta_path);
    {
        char authoring_path[LINE_DRAWING_PATH_CAP];
        char runtime_path[LINE_DRAWING_PATH_CAP];
        snprintf(authoring_path, sizeof(authoring_path), "%s/scene_authoring.json", scene_dir);
        snprintf(runtime_path, sizeof(runtime_path), "%s/scene_runtime.json", scene_dir);
        (void)unlink(authoring_path);
        (void)unlink(runtime_path);
    }
    (void)rmdir(scene_dir);
    (void)rmdir(temp_root);
    return true;
}

static bool test_file_browser_restore_reopens_last_layout_for_json_mode(void) {
    UIPanelState* ui = NULL;
    char temp_root[LINE_DRAWING_PATH_CAP];
    char alpha_path[LINE_DRAWING_PATH_CAP];

    ld_test_remove_file_browser_runtime_state();
    snprintf(temp_root, sizeof(temp_root), "/tmp/ld_file_browser_restore_%u", (unsigned)SDL_GetTicks());
    TEST_ASSERT(mkdir(temp_root, 0755) == 0 || errno == EEXIST);
    snprintf(alpha_path, sizeof(alpha_path), "%s/alpha.json", temp_root);

    ld_test_init_runtime();
    TEST_ASSERT(ld_test_write_layout_json(alpha_path));
    TEST_ASSERT(Global_SetInputRoot(temp_root, true));
    UIPanel_ActivateJsonBrowser();
    TEST_ASSERT(UIPanel_LoadLayoutFromPath(alpha_path));
    ld_test_shutdown_runtime();

    ld_test_init_runtime();
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    TEST_ASSERT(ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON);
    TEST_ASSERT(UIPanel_RestorePersistedFileSession());
    TEST_ASSERT(strcmp(Global_GetInputRoot(), temp_root) == 0);
    TEST_ASSERT(strcmp(Global_GetCurrentConfigPath(), alpha_path) == 0);
    TEST_ASSERT(ui->loadMenu.activeIndex == ld_test_find_load_menu_index(ui, "alpha.json"));
    ld_test_shutdown_runtime();

    ld_test_remove_file_browser_runtime_state();
    (void)unlink(alpha_path);
    (void)rmdir(temp_root);
    return true;
}

static bool test_file_browser_remembers_last_browsed_entry_without_session_restore(void) {
    UIPanelState* ui = NULL;
    char temp_root[LINE_DRAWING_PATH_CAP];
    char alpha_path[LINE_DRAWING_PATH_CAP];
    char beta_path[LINE_DRAWING_PATH_CAP];
    int beta_index = -1;

    ld_test_remove_file_browser_runtime_state();
    snprintf(temp_root, sizeof(temp_root), "/tmp/ld_file_browser_remember_%u", (unsigned)SDL_GetTicks());
    TEST_ASSERT(mkdir(temp_root, 0755) == 0 || errno == EEXIST);
    snprintf(alpha_path, sizeof(alpha_path), "%s/alpha.json", temp_root);
    snprintf(beta_path, sizeof(beta_path), "%s/beta.json", temp_root);

    ld_test_init_runtime();
    TEST_ASSERT(ld_test_write_layout_json(alpha_path));
    TEST_ASSERT(ld_test_write_layout_json(beta_path));
    TEST_ASSERT(Global_SetInputRoot(temp_root, true));
    UIPanel_ActivateJsonBrowser();
    TEST_ASSERT(UIPanel_LoadLayoutFromPath(beta_path));
    ld_test_shutdown_runtime();

    ld_test_init_runtime();
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    TEST_ASSERT(ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON);
    TEST_ASSERT(strcmp(Global_GetInputRoot(), temp_root) == 0);
    TEST_ASSERT(strcmp(Global_GetCurrentConfigPath(), beta_path) != 0);
    UIPanel_ActivateJsonBrowser();
    beta_index = ld_test_find_load_menu_index(ui, "beta.json");
    TEST_ASSERT(beta_index >= 0);
    TEST_ASSERT(ui->loadMenu.activeIndex == beta_index);
    ld_test_shutdown_runtime();

    ld_test_remove_file_browser_runtime_state();
    (void)unlink(alpha_path);
    (void)unlink(beta_path);
    (void)rmdir(temp_root);
    return true;
}

static bool test_file_browser_root_switch_clears_unmatched_highlight(void) {
    UIPanelState* ui = NULL;
    char root_a[LINE_DRAWING_PATH_CAP];
    char root_b[LINE_DRAWING_PATH_CAP];
    char alpha_path[LINE_DRAWING_PATH_CAP];
    char gamma_path[LINE_DRAWING_PATH_CAP];
    UILoadMenuSelectionState selection_state = UI_LOAD_MENU_SELECTION_NONE;
    const char* selection_path = NULL;

    ld_test_remove_file_browser_runtime_state();
    snprintf(root_a, sizeof(root_a), "/tmp/ld_file_browser_root_a_%u", (unsigned)SDL_GetTicks());
    snprintf(root_b, sizeof(root_b), "/tmp/ld_file_browser_root_b_%u", (unsigned)SDL_GetTicks() + 1u);
    TEST_ASSERT(mkdir(root_a, 0755) == 0 || errno == EEXIST);
    TEST_ASSERT(mkdir(root_b, 0755) == 0 || errno == EEXIST);
    snprintf(alpha_path, sizeof(alpha_path), "%s/alpha.json", root_a);
    snprintf(gamma_path, sizeof(gamma_path), "%s/gamma.json", root_b);

    ld_test_init_runtime();
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    TEST_ASSERT(ld_test_write_layout_json(alpha_path));
    TEST_ASSERT(ld_test_write_layout_json(gamma_path));
    TEST_ASSERT(Global_SetInputRoot(root_a, true));
    UIPanel_ActivateJsonBrowser();
    TEST_ASSERT(UIPanel_LoadLayoutFromPath(alpha_path));
    TEST_ASSERT(ui->loadMenu.activeIndex == ld_test_find_load_menu_index(ui, "alpha.json"));
    TEST_ASSERT(Global_SetInputRoot(root_b, true));
    UIPanel_ActivateJsonBrowser();
    TEST_ASSERT(ui->loadMenu.count == 1);
    TEST_ASSERT(ui->loadMenu.activeIndex == -1);
    TEST_ASSERT(!UIPanel_GetFileBrowserSelectionInfo(ui, &selection_state, &selection_path));
    TEST_ASSERT(selection_state == UI_LOAD_MENU_SELECTION_NONE);
    TEST_ASSERT(selection_path == NULL);
    ld_test_shutdown_runtime();

    ld_test_remove_file_browser_runtime_state();
    (void)unlink(alpha_path);
    (void)unlink(gamma_path);
    (void)rmdir(root_a);
    (void)rmdir(root_b);
    return true;
}

static bool test_file_browser_mode_specific_roots_restore_on_activation(void) {
    UIPanelState* ui = NULL;
    char json_root[LINE_DRAWING_PATH_CAP];
    char scene_root[LINE_DRAWING_PATH_CAP];
    char alpha_path[LINE_DRAWING_PATH_CAP];
    char scene_dir[LINE_DRAWING_PATH_CAP];

    ld_test_remove_file_browser_runtime_state();
    snprintf(json_root, sizeof(json_root), "/tmp/ld_file_browser_json_root_%u", (unsigned)SDL_GetTicks());
    snprintf(scene_root, sizeof(scene_root), "/tmp/ld_file_browser_scene_root_%u", (unsigned)SDL_GetTicks() + 1u);
    TEST_ASSERT(mkdir(json_root, 0755) == 0 || errno == EEXIST);
    TEST_ASSERT(mkdir(scene_root, 0755) == 0 || errno == EEXIST);
    snprintf(alpha_path, sizeof(alpha_path), "%s/alpha.json", json_root);
    snprintf(scene_dir, sizeof(scene_dir), "%s/scene_one", scene_root);

    ld_test_init_runtime();
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    TEST_ASSERT(ld_test_write_layout_json(alpha_path));
    TEST_ASSERT(ld_test_write_scene_contract(scene_dir));

    TEST_ASSERT(UIPanel_LoadJsonFromFolderSelection(json_root, true));
    TEST_ASSERT(ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON);
    TEST_ASSERT(strcmp(ui->loadMenu.rootPath, json_root) == 0);
    TEST_ASSERT(ui->loadMenu.count == 1);

    TEST_ASSERT(UIPanel_LoadSceneFromFolderSelection(scene_root, true));
    TEST_ASSERT(ui->loadMenu.mode == UI_LOAD_MENU_MODE_SCENE);
    TEST_ASSERT(strcmp(ui->loadMenu.rootPath, scene_root) == 0);
    TEST_ASSERT(ui->loadMenu.count == 1);

    TEST_ASSERT(Global_SetInputRoot(json_root, true));
    UIPanel_ActivateSceneBrowser();
    TEST_ASSERT(ui->loadMenu.mode == UI_LOAD_MENU_MODE_SCENE);
    TEST_ASSERT(strcmp(ui->loadMenu.rootPath, scene_root) == 0);
    TEST_ASSERT(ui->loadMenu.count == 1);

    UIPanel_ActivateJsonBrowser();
    TEST_ASSERT(ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON);
    TEST_ASSERT(strcmp(ui->loadMenu.rootPath, json_root) == 0);
    TEST_ASSERT(ui->loadMenu.count == 1);

    ld_test_shutdown_runtime();

    ld_test_remove_file_browser_runtime_state();
    (void)unlink(alpha_path);
    {
        char authoring_path[LINE_DRAWING_PATH_CAP];
        char runtime_path[LINE_DRAWING_PATH_CAP];
        snprintf(authoring_path, sizeof(authoring_path), "%s/scene_authoring.json", scene_dir);
        snprintf(runtime_path, sizeof(runtime_path), "%s/scene_runtime.json", scene_dir);
        (void)unlink(authoring_path);
        (void)unlink(runtime_path);
    }
    (void)rmdir(scene_dir);
    (void)rmdir(json_root);
    (void)rmdir(scene_root);
    return true;
}

static bool test_session_input_root_change_does_not_override_mode_browser_root(void) {
    UIPanelState* ui = NULL;
    char json_root[LINE_DRAWING_PATH_CAP];
    char session_root[LINE_DRAWING_PATH_CAP];
    char alpha_path[LINE_DRAWING_PATH_CAP];
    char beta_path[LINE_DRAWING_PATH_CAP];

    ld_test_remove_file_browser_runtime_state();
    snprintf(json_root, sizeof(json_root), "/tmp/ld_file_browser_mode_root_%u", (unsigned)SDL_GetTicks());
    snprintf(session_root, sizeof(session_root), "/tmp/ld_file_browser_session_root_%u", (unsigned)SDL_GetTicks() + 1u);
    TEST_ASSERT(mkdir(json_root, 0755) == 0 || errno == EEXIST);
    TEST_ASSERT(mkdir(session_root, 0755) == 0 || errno == EEXIST);
    snprintf(alpha_path, sizeof(alpha_path), "%s/alpha.json", json_root);
    snprintf(beta_path, sizeof(beta_path), "%s/beta.json", session_root);

    ld_test_init_runtime();
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    TEST_ASSERT(ld_test_write_layout_json(alpha_path));
    TEST_ASSERT(ld_test_write_layout_json(beta_path));

    TEST_ASSERT(UIPanel_LoadJsonFromFolderSelection(json_root, true));
    TEST_ASSERT(ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON);
    TEST_ASSERT(strcmp(ui->loadMenu.rootPath, json_root) == 0);
    TEST_ASSERT(ui->loadMenu.count == 1);

    TEST_ASSERT(Global_SetInputRoot(session_root, true));
    TEST_ASSERT(strcmp(Global_GetInputRoot(), session_root) == 0);
    TEST_ASSERT(strcmp(ui->loadMenu.rootPath, json_root) == 0);
    TEST_ASSERT(ui->loadMenu.count == 1);
    TEST_ASSERT(ld_test_find_load_menu_index(ui, "alpha.json") >= 0);
    TEST_ASSERT(ld_test_find_load_menu_index(ui, "beta.json") < 0);
    ld_test_shutdown_runtime();

    ld_test_remove_file_browser_runtime_state();
    (void)unlink(alpha_path);
    (void)unlink(beta_path);
    (void)rmdir(json_root);
    (void)rmdir(session_root);
    return true;
}

bool ui_panel_file_browser_run_tests(void) {
    const TestCase cases[] = {
        { "file_browser_switches_modes_and_uses_pane_rect",
          test_file_browser_switches_modes_and_uses_pane_rect },
        { "file_browser_restore_reopens_last_layout_for_json_mode",
          test_file_browser_restore_reopens_last_layout_for_json_mode },
        { "file_browser_remembers_last_browsed_entry_without_session_restore",
          test_file_browser_remembers_last_browsed_entry_without_session_restore },
        { "file_browser_root_switch_clears_unmatched_highlight",
          test_file_browser_root_switch_clears_unmatched_highlight },
        { "file_browser_mode_specific_roots_restore_on_activation",
          test_file_browser_mode_specific_roots_restore_on_activation },
        { "session_input_root_change_does_not_override_mode_browser_root",
          test_session_input_root_change_does_not_override_mode_browser_root },
    };
    return run_test_cases("UIPanelFileBrowser", cases, sizeof(cases) / sizeof(cases[0]));
}
