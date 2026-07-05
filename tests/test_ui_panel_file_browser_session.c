#include "test_layout_internal.h"
#include "test_artifact_helpers.h"

#include "UI/ui_panel.h"
#include "UI/ui_panel_internal.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void ld_test_remove_file_browser_runtime_state(void) {
    ld_test_artifact_clear_runtime_state_files();
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
    return ld_test_artifact_write_scene_contract(scene_dir,
                                                 "{\"schema\":\"scene_authoring_v1\"}\n",
                                                 "{\"schema\":\"scene_runtime_v1\"}\n");
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
    UIPanel_OnWindowResized(Global_Get()->screenWidth, Global_Get()->screenHeight);
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

static bool test_session_input_root_change_preserves_loaded_layout_identity(void) {
    UIPanelState* ui = NULL;
    char layout_root[LINE_DRAWING_PATH_CAP];
    char session_root[LINE_DRAWING_PATH_CAP];
    char alpha_path[LINE_DRAWING_PATH_CAP];

    ld_test_remove_file_browser_runtime_state();
    snprintf(layout_root, sizeof(layout_root), "/tmp/ld_input_root_identity_a_%u", (unsigned)SDL_GetTicks());
    snprintf(session_root, sizeof(session_root), "/tmp/ld_input_root_identity_b_%u", (unsigned)SDL_GetTicks() + 1u);
    TEST_ASSERT(mkdir(layout_root, 0755) == 0 || errno == EEXIST);
    TEST_ASSERT(mkdir(session_root, 0755) == 0 || errno == EEXIST);
    snprintf(alpha_path, sizeof(alpha_path), "%s/alpha.json", layout_root);

    ld_test_init_runtime();
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    TEST_ASSERT(ld_test_write_layout_json(alpha_path));
    TEST_ASSERT(UIPanel_LoadJsonFromFolderSelection(layout_root, true));
    TEST_ASSERT(UIPanel_LoadLayoutFromPath(alpha_path));
    TEST_ASSERT(strcmp(Global_GetCurrentConfigPath(), alpha_path) == 0);
    TEST_ASSERT(strcmp(Global_GetLastLayoutPath(), alpha_path) == 0);

    TEST_ASSERT(Global_SetInputRoot(session_root, true));
    TEST_ASSERT(strcmp(Global_GetInputRoot(), session_root) == 0);
    TEST_ASSERT(strcmp(Global_GetCurrentConfigPath(), alpha_path) == 0);
    TEST_ASSERT(strcmp(Global_GetLastLayoutPath(), alpha_path) == 0);
    ld_test_shutdown_runtime();

    ld_test_remove_file_browser_runtime_state();
    (void)unlink(alpha_path);
    (void)rmdir(layout_root);
    (void)rmdir(session_root);
    return true;
}

static bool test_session_input_root_change_updates_default_layout_path_before_load(void) {
    char root_a[LINE_DRAWING_PATH_CAP];
    char root_b[LINE_DRAWING_PATH_CAP];
    char expected_default_path[LINE_DRAWING_PATH_CAP];

    ld_test_remove_file_browser_runtime_state();
    snprintf(root_a, sizeof(root_a), "/tmp/ld_input_root_default_a_%u", (unsigned)SDL_GetTicks());
    snprintf(root_b, sizeof(root_b), "/tmp/ld_input_root_default_b_%u", (unsigned)SDL_GetTicks() + 1u);
    TEST_ASSERT(mkdir(root_a, 0755) == 0 || errno == EEXIST);
    TEST_ASSERT(mkdir(root_b, 0755) == 0 || errno == EEXIST);

    ld_test_init_runtime();
    TEST_ASSERT(Global_SetInputRoot(root_a, true));
    TEST_ASSERT(Global_SetInputRoot(root_b, true));
    TEST_ASSERT(snprintf(expected_default_path,
                         sizeof(expected_default_path),
                         "%s/layout_config.json",
                         root_b) < (int)sizeof(expected_default_path));
    TEST_ASSERT(strcmp(Global_GetCurrentConfigPath(), expected_default_path) == 0);
    TEST_ASSERT(strcmp(Global_GetLastLayoutPath(), expected_default_path) == 0);
    ld_test_shutdown_runtime();

    ld_test_remove_file_browser_runtime_state();
    (void)rmdir(root_a);
    (void)rmdir(root_b);
    return true;
}

static bool test_file_browser_status_text_marks_active_session_row(void) {
    UIPanelState* ui = NULL;
    char temp_root[LINE_DRAWING_PATH_CAP];
    char alpha_path[LINE_DRAWING_PATH_CAP];
    char status_text[256];
    char action_text[320];
    UILoadMenuSelectionState row_state = UI_LOAD_MENU_SELECTION_NONE;
    int alpha_index = -1;

    ld_test_remove_file_browser_runtime_state();
    snprintf(temp_root, sizeof(temp_root), "/tmp/ld_file_browser_status_active_%u", (unsigned)SDL_GetTicks());
    TEST_ASSERT(mkdir(temp_root, 0755) == 0 || errno == EEXIST);
    snprintf(alpha_path, sizeof(alpha_path), "%s/alpha.json", temp_root);

    ld_test_init_runtime();
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    TEST_ASSERT(ld_test_write_layout_json(alpha_path));
    TEST_ASSERT(UIPanel_LoadJsonFromFolderSelection(temp_root, true));
    TEST_ASSERT(UIPanel_LoadLayoutFromPath(alpha_path));
    alpha_index = ld_test_find_load_menu_index(ui, "alpha.json");
    TEST_ASSERT(alpha_index >= 0);

    TEST_ASSERT(UIPanel_GetFileBrowserStatusText(ui, status_text, sizeof(status_text)));
    TEST_ASSERT(strstr(status_text, "Active row alpha.json") != NULL);
    TEST_ASSERT(UIPanel_GetFileBrowserActionHintText(ui, action_text, sizeof(action_text)));
    TEST_ASSERT(strstr(action_text, "re-centers the live row") != NULL);
    TEST_ASSERT(strstr(action_text, "Clear Last is only for remembered fallback rows") != NULL);
    TEST_ASSERT(UIPanel_GetFileBrowserRowSelectionState(ui, alpha_index, &row_state));
    TEST_ASSERT(row_state == UI_LOAD_MENU_SELECTION_ACTIVE_SESSION);
    ld_test_shutdown_runtime();

    ld_test_remove_file_browser_runtime_state();
    (void)unlink(alpha_path);
    (void)rmdir(temp_root);
    return true;
}

static bool test_file_browser_status_text_marks_remembered_fallback_row(void) {
    UIPanelState* ui = NULL;
    char temp_root[LINE_DRAWING_PATH_CAP];
    char alpha_path[LINE_DRAWING_PATH_CAP];
    char status_text[256];
    char action_text[320];
    UILoadMenuSelectionState row_state = UI_LOAD_MENU_SELECTION_NONE;
    int alpha_index = -1;

    ld_test_remove_file_browser_runtime_state();
    snprintf(temp_root, sizeof(temp_root), "/tmp/ld_file_browser_status_remembered_%u", (unsigned)SDL_GetTicks());
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
    UIPanel_ActivateJsonBrowser();
    alpha_index = ld_test_find_load_menu_index(ui, "alpha.json");
    TEST_ASSERT(alpha_index >= 0);
    TEST_ASSERT(UIPanel_GetFileBrowserStatusText(ui, status_text, sizeof(status_text)));
    TEST_ASSERT(strstr(status_text, "Remembered row alpha.json") != NULL);
    TEST_ASSERT(UIPanel_GetFileBrowserActionHintText(ui, action_text, sizeof(action_text)));
    TEST_ASSERT(strstr(action_text, "restores the live session row") != NULL);
    TEST_ASSERT(strstr(action_text, "removes this remembered fallback row") != NULL);
    TEST_ASSERT(UIPanel_GetFileBrowserRowSelectionState(ui, alpha_index, &row_state));
    TEST_ASSERT(row_state == UI_LOAD_MENU_SELECTION_REMEMBERED_ENTRY);
    ld_test_shutdown_runtime();

    ld_test_remove_file_browser_runtime_state();
    (void)unlink(alpha_path);
    (void)rmdir(temp_root);
    return true;
}

static bool test_file_browser_status_text_marks_unmatched_root_state(void) {
    UIPanelState* ui = NULL;
    char root_a[LINE_DRAWING_PATH_CAP];
    char root_b[LINE_DRAWING_PATH_CAP];
    char alpha_path[LINE_DRAWING_PATH_CAP];
    char gamma_path[LINE_DRAWING_PATH_CAP];
    char status_text[256];
    char action_text[320];
    UILoadMenuSelectionState row_state = UI_LOAD_MENU_SELECTION_ACTIVE_SESSION;
    int gamma_index = -1;

    ld_test_remove_file_browser_runtime_state();
    snprintf(root_a, sizeof(root_a), "/tmp/ld_file_browser_status_unmatched_a_%u", (unsigned)SDL_GetTicks());
    snprintf(root_b, sizeof(root_b), "/tmp/ld_file_browser_status_unmatched_b_%u", (unsigned)SDL_GetTicks() + 1u);
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
    TEST_ASSERT(Global_SetInputRoot(root_b, true));
    UIPanel_ActivateJsonBrowser();
    gamma_index = ld_test_find_load_menu_index(ui, "gamma.json");
    TEST_ASSERT(gamma_index >= 0);

    TEST_ASSERT(UIPanel_GetFileBrowserStatusText(ui, status_text, sizeof(status_text)));
    TEST_ASSERT(strstr(status_text, "JSON mode has entries but no active row") != NULL);
    TEST_ASSERT(UIPanel_GetFileBrowserActionHintText(ui, action_text, sizeof(action_text)));
    TEST_ASSERT(strstr(action_text, "Use Session targets the live session row") != NULL);
    TEST_ASSERT(strstr(action_text, "stale remembered fallback") != NULL);
    TEST_ASSERT(!UIPanel_GetFileBrowserRowSelectionState(ui, gamma_index, &row_state));
    TEST_ASSERT(row_state == UI_LOAD_MENU_SELECTION_NONE);
    ld_test_shutdown_runtime();

    ld_test_remove_file_browser_runtime_state();
    (void)unlink(alpha_path);
    (void)unlink(gamma_path);
    (void)rmdir(root_a);
    (void)rmdir(root_b);
    return true;
}

static bool test_file_browser_use_active_realigns_browser_root_and_highlight(void) {
    UIPanelState* ui = NULL;
    char root_a[LINE_DRAWING_PATH_CAP];
    char root_b[LINE_DRAWING_PATH_CAP];
    char alpha_path[LINE_DRAWING_PATH_CAP];
    char gamma_path[LINE_DRAWING_PATH_CAP];
    UILoadMenuSelectionState selection_state = UI_LOAD_MENU_SELECTION_NONE;
    const char* selection_path = NULL;

    ld_test_remove_file_browser_runtime_state();
    snprintf(root_a, sizeof(root_a), "/tmp/ld_file_browser_use_active_a_%u", (unsigned)SDL_GetTicks());
    snprintf(root_b, sizeof(root_b), "/tmp/ld_file_browser_use_active_b_%u", (unsigned)SDL_GetTicks() + 1u);
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

    TEST_ASSERT(UIPanel_LoadJsonFromFolderSelection(root_b, true));
    TEST_ASSERT(strcmp(ui->loadMenu.rootPath, root_b) == 0);
    TEST_ASSERT(ui->loadMenu.activeIndex == -1);

    TEST_ASSERT(UIPanel_FocusFileBrowserOnActiveSession());
    TEST_ASSERT(strcmp(ui->loadMenu.rootPath, root_a) == 0);
    TEST_ASSERT(ui->loadMenu.activeIndex == ld_test_find_load_menu_index(ui, "alpha.json"));
    TEST_ASSERT(UIPanel_GetFileBrowserSelectionInfo(ui, &selection_state, &selection_path));
    TEST_ASSERT(selection_state == UI_LOAD_MENU_SELECTION_ACTIVE_SESSION);
    TEST_ASSERT(selection_path != NULL && strcmp(selection_path, alpha_path) == 0);
    ld_test_shutdown_runtime();

    ld_test_remove_file_browser_runtime_state();
    (void)unlink(alpha_path);
    (void)unlink(gamma_path);
    (void)rmdir(root_a);
    (void)rmdir(root_b);
    return true;
}

static bool test_file_browser_clear_last_removes_remembered_fallback(void) {
    UIPanelState* ui = NULL;
    char temp_root[LINE_DRAWING_PATH_CAP];
    char alpha_path[LINE_DRAWING_PATH_CAP];
    char beta_path[LINE_DRAWING_PATH_CAP];
    int beta_index = -1;
    UILoadMenuSelectionState selection_state = UI_LOAD_MENU_SELECTION_NONE;
    const char* selection_path = NULL;

    ld_test_remove_file_browser_runtime_state();
    snprintf(temp_root, sizeof(temp_root), "/tmp/ld_file_browser_clear_last_%u", (unsigned)SDL_GetTicks());
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
    UIPanel_ActivateJsonBrowser();
    beta_index = ld_test_find_load_menu_index(ui, "beta.json");
    TEST_ASSERT(beta_index >= 0);
    TEST_ASSERT(ui->loadMenu.activeIndex == beta_index);
    TEST_ASSERT(UIPanel_GetFileBrowserSelectionInfo(ui, &selection_state, &selection_path));
    TEST_ASSERT(selection_state == UI_LOAD_MENU_SELECTION_REMEMBERED_ENTRY);
    TEST_ASSERT(selection_path != NULL && strcmp(selection_path, beta_path) == 0);

    TEST_ASSERT(UIPanel_ClearRememberedFileBrowserEntry());
    TEST_ASSERT(ui->loadMenu.activeIndex == -1);
    TEST_ASSERT(!UIPanel_GetFileBrowserSelectionInfo(ui, &selection_state, &selection_path));
    TEST_ASSERT(selection_state == UI_LOAD_MENU_SELECTION_NONE);
    TEST_ASSERT(selection_path == NULL);
    ld_test_shutdown_runtime();

    ld_test_remove_file_browser_runtime_state();
    (void)unlink(alpha_path);
    (void)unlink(beta_path);
    (void)rmdir(temp_root);
    return true;
}

static bool test_file_browser_restore_summary_reports_active_and_remembered_json(void) {
    UIPanelState* ui = NULL;
    UIPanelFileBrowserRestoreSummary summary;
    char root[LINE_DRAWING_PATH_CAP];
    char alpha_path[LINE_DRAWING_PATH_CAP];
    int alpha_index = -1;

    ld_test_remove_file_browser_runtime_state();
    snprintf(root, sizeof(root), "/tmp/ld_file_browser_restore_summary_%u", (unsigned)SDL_GetTicks());
    TEST_ASSERT(mkdir(root, 0755) == 0 || errno == EEXIST);
    snprintf(alpha_path, sizeof(alpha_path), "%s/alpha.json", root);

    ld_test_init_runtime();
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    TEST_ASSERT(ld_test_write_layout_json(alpha_path));
    TEST_ASSERT(Global_SetInputRoot(root, true));
    UIPanel_ActivateJsonBrowser();
    TEST_ASSERT(UIPanel_LoadLayoutFromPath(alpha_path));
    alpha_index = ld_test_find_load_menu_index(ui, "alpha.json");
    TEST_ASSERT(alpha_index >= 0);

    TEST_ASSERT(UIPanel_GetFileBrowserRestoreSummary(ui, &summary));
    TEST_ASSERT(summary.mode == UI_LOAD_MENU_MODE_JSON);
    TEST_ASSERT(summary.hasMode);
    TEST_ASSERT(summary.visible);
    TEST_ASSERT(strcmp(summary.rootPath, root) == 0);
    TEST_ASSERT(summary.hasActiveSessionPath);
    TEST_ASSERT(summary.activeSessionPathExists);
    TEST_ASSERT(strcmp(summary.activeSessionPath, alpha_path) == 0);
    TEST_ASSERT(summary.activeIndex == alpha_index);
    TEST_ASSERT(summary.hasRememberedEntryPath);
    TEST_ASSERT(summary.rememberedEntryExists);
    TEST_ASSERT(strcmp(summary.rememberedEntryPath, alpha_path) == 0);
    TEST_ASSERT(summary.rememberedIndex == alpha_index);

    ld_test_shutdown_runtime();
    ld_test_remove_file_browser_runtime_state();
    (void)unlink(alpha_path);
    (void)rmdir(root);
    return true;
}

static bool test_file_browser_restore_summary_reports_stale_remembered_entry(void) {
    UIPanelState* ui = NULL;
    UIPanelFileBrowserRestoreSummary summary;
    char root[LINE_DRAWING_PATH_CAP];
    char alpha_path[LINE_DRAWING_PATH_CAP];
    char stale_path[LINE_DRAWING_PATH_CAP];

    ld_test_remove_file_browser_runtime_state();
    snprintf(root, sizeof(root), "/tmp/ld_file_browser_restore_stale_%u", (unsigned)SDL_GetTicks());
    TEST_ASSERT(mkdir(root, 0755) == 0 || errno == EEXIST);
    snprintf(alpha_path, sizeof(alpha_path), "%s/alpha.json", root);
    snprintf(stale_path, sizeof(stale_path), "%s/missing.json", root);

    ld_test_init_runtime();
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    TEST_ASSERT(ld_test_write_layout_json(alpha_path));
    TEST_ASSERT(Global_SetInputRoot(root, true));
    UIPanel_ActivateJsonBrowser();
    UIPanel_RememberLoadedEntry(UI_LOAD_MENU_MODE_JSON, stale_path);
    UIPanel_RefreshConfigList();

    TEST_ASSERT(UIPanel_GetFileBrowserRestoreSummary(ui, &summary));
    TEST_ASSERT(summary.mode == UI_LOAD_MENU_MODE_JSON);
    TEST_ASSERT(summary.hasRememberedEntryPath);
    TEST_ASSERT(!summary.rememberedEntryExists);
    TEST_ASSERT(strcmp(summary.rememberedEntryPath, stale_path) == 0);
    TEST_ASSERT(summary.rememberedIndex == -1);
    TEST_ASSERT(summary.hasActiveSessionPath);
    TEST_ASSERT(!summary.activeSessionPathExists);
    TEST_ASSERT(summary.activeIndex == -1);

    ld_test_shutdown_runtime();
    ld_test_remove_file_browser_runtime_state();
    (void)unlink(alpha_path);
    (void)rmdir(root);
    return true;
}

bool ui_panel_file_browser_session_run_tests(void) {
    const TestCase cases[] = {
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
        { "session_input_root_change_preserves_loaded_layout_identity",
          test_session_input_root_change_preserves_loaded_layout_identity },
        { "session_input_root_change_updates_default_layout_path_before_load",
          test_session_input_root_change_updates_default_layout_path_before_load },
        { "file_browser_status_text_marks_active_session_row",
          test_file_browser_status_text_marks_active_session_row },
        { "file_browser_status_text_marks_remembered_fallback_row",
          test_file_browser_status_text_marks_remembered_fallback_row },
        { "file_browser_status_text_marks_unmatched_root_state",
          test_file_browser_status_text_marks_unmatched_root_state },
        { "file_browser_use_active_realigns_browser_root_and_highlight",
          test_file_browser_use_active_realigns_browser_root_and_highlight },
        { "file_browser_clear_last_removes_remembered_fallback",
          test_file_browser_clear_last_removes_remembered_fallback },
        { "file_browser_restore_summary_reports_active_and_remembered_json",
          test_file_browser_restore_summary_reports_active_and_remembered_json },
        { "file_browser_restore_summary_reports_stale_remembered_entry",
          test_file_browser_restore_summary_reports_stale_remembered_entry },
    };
    return run_test_cases("UIPanelFileBrowserSession",
                          cases,
                          sizeof(cases) / sizeof(cases[0]));
}
