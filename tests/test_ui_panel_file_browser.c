#include "test_layout_internal.h"

#include "UI/ui_panel.h"
#include "UI/ui_panel_internal.h"
#include "UI/ui_panel_file_layout.h"
#include "UI/panel/ui_panel_file_browser_internal.h"
#include "Core/line_drawing_file_catalog.h"
#include "Layout/asset/layout_imported_mesh_asset.h"
#include "Layout/scene/layout_mesh_preview_sidecar.h"
#include "Layout/scene/layout_mesh_runtime_preview.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

static void ld_test_remove_file_browser_runtime_state(void) {
    (void)unlink("data/runtime/file_browser_mode.txt");
    (void)unlink("data/runtime/file_browser_json_root.txt");
    (void)unlink("data/runtime/file_browser_scene_root.txt");
    (void)unlink("data/runtime/file_browser_object_root.txt");
    (void)unlink("data/runtime/file_browser_mesh_root.txt");
    (void)unlink("data/runtime/file_browser_stl_root.txt");
    (void)unlink("data/runtime/file_browser_last_json_entry.txt");
    (void)unlink("data/runtime/file_browser_last_scene_entry.txt");
    (void)unlink("data/runtime/file_browser_last_object_entry.txt");
    (void)unlink("data/runtime/file_browser_last_mesh_entry.txt");
    (void)unlink("data/runtime/file_browser_last_stl_entry.txt");
    (void)unlink("data/runtime/recent_layouts.txt");
    (void)unlink("data/runtime/recent_scenes.txt");
    (void)unlink("data/runtime/recent_object_assets.txt");
    (void)unlink("data/runtime/recent_input_roots.txt");
    (void)unlink("data/runtime/recent_output_roots.txt");
    (void)unlink("data/runtime/input_root.txt");
    (void)unlink("data/runtime/output_root.txt");
    (void)unlink("data/runtime/layout_root.txt");
    (void)unlink("data/runtime/object_asset_root.txt");
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

static bool ld_test_write_runtime_mesh_sidecar(const char* path, const char* asset_id) {
    char json[768];
    if (!path || !path[0]) return false;
    if (!asset_id || !asset_id[0]) asset_id = "test_mesh";
    snprintf(json,
             sizeof(json),
             "{"
             "\"schema_variant\":\"mesh_asset_runtime_v1\","
             "\"asset_id\":\"%s\","
             "\"source_asset_id\":\"%s_source\","
             "\"vertex_count\":4,"
             "\"triangle_count\":2,"
             "\"local_bounds\":{"
             "\"min\":{\"x\":-1.0,\"y\":-1.0,\"z\":-1.0},"
             "\"max\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}"
             "}"
             "}\n",
             asset_id,
             asset_id);
    return ld_test_write_text_file_basic(path, json);
}

static bool ld_test_write_tetrahedron_stl(const char* path) {
    const char* text =
        "solid tetra\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex 0 0 0\n"
        "      vertex 1 0 0\n"
        "      vertex 0 1 0\n"
        "    endloop\n"
        "  endfacet\n"
        "  facet normal 0 1 0\n"
        "    outer loop\n"
        "      vertex 0 0 0\n"
        "      vertex 0 0 1\n"
        "      vertex 1 0 0\n"
        "    endloop\n"
        "  endfacet\n"
        "  facet normal 1 0 0\n"
        "    outer loop\n"
        "      vertex 0 0 0\n"
        "      vertex 0 1 0\n"
        "      vertex 0 0 1\n"
        "    endloop\n"
        "  endfacet\n"
        "  facet normal 1 1 1\n"
        "    outer loop\n"
        "      vertex 1 0 0\n"
        "      vertex 0 0 1\n"
        "      vertex 0 1 0\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid tetra\n";
    return ld_test_write_text_file_basic(path, text);
}

static bool ld_test_write_oversized_tetrahedron_stl(const char* path) {
    const char* text =
        "solid oversized_tetra\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex -1000 -1000 0\n"
        "      vertex 1000 -1000 0\n"
        "      vertex -1000 1000 0\n"
        "    endloop\n"
        "  endfacet\n"
        "  facet normal 0 1 0\n"
        "    outer loop\n"
        "      vertex -1000 -1000 0\n"
        "      vertex -1000 -1000 2000\n"
        "      vertex 1000 -1000 0\n"
        "    endloop\n"
        "  endfacet\n"
        "  facet normal 1 0 0\n"
        "    outer loop\n"
        "      vertex -1000 -1000 0\n"
        "      vertex -1000 1000 0\n"
        "      vertex -1000 -1000 2000\n"
        "    endloop\n"
        "  endfacet\n"
        "  facet normal 1 1 1\n"
        "    outer loop\n"
        "      vertex 1000 -1000 0\n"
        "      vertex -1000 -1000 2000\n"
        "      vertex -1000 1000 0\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid oversized_tetra\n";
    return ld_test_write_text_file_basic(path, text);
}

static void ld_test_write_u32_le(FILE* f, uint32_t value) {
    fputc((int)(value & 0xffu), f);
    fputc((int)((value >> 8u) & 0xffu), f);
    fputc((int)((value >> 16u) & 0xffu), f);
    fputc((int)((value >> 24u) & 0xffu), f);
}

static void ld_test_write_f32_le(FILE* f, float value) {
    union {
        float f;
        uint32_t u;
    } conv;
    conv.f = value;
    ld_test_write_u32_le(f, conv.u);
}

static bool ld_test_write_repeated_triangle_binary_stl(const char* path, uint32_t triangle_count) {
    FILE* f = NULL;
    unsigned char header[80] = {0};
    if (!path || triangle_count == 0u) return false;
    f = fopen(path, "wb");
    if (!f) return false;
    memcpy(header, "line drawing large preview regression", 37u);
    if (fwrite(header, 1u, sizeof(header), f) != sizeof(header)) {
        fclose(f);
        return false;
    }
    ld_test_write_u32_le(f, triangle_count);
    for (uint32_t i = 0u; i < triangle_count; ++i) {
        ld_test_write_f32_le(f, 0.0f);
        ld_test_write_f32_le(f, 0.0f);
        ld_test_write_f32_le(f, 1.0f);
        ld_test_write_f32_le(f, 0.0f);
        ld_test_write_f32_le(f, 0.0f);
        ld_test_write_f32_le(f, 0.0f);
        ld_test_write_f32_le(f, 1.0f);
        ld_test_write_f32_le(f, 0.0f);
        ld_test_write_f32_le(f, 0.0f);
        ld_test_write_f32_le(f, 0.0f);
        ld_test_write_f32_le(f, 1.0f);
        ld_test_write_f32_le(f, 0.0f);
        fputc(0, f);
        fputc(0, f);
    }
    return fclose(f) == 0;
}

static bool test_file_browser_switches_modes_and_uses_pane_rect(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    char temp_root[LINE_DRAWING_PATH_CAP];
    char alpha_path[LINE_DRAWING_PATH_CAP];
    char beta_path[LINE_DRAWING_PATH_CAP];
    char scene_dir[LINE_DRAWING_PATH_CAP];
    SDL_Rect browser_rect = {0};
    SDL_Rect list_clip = {0};
    SDL_Rect set_dir_button = {0};

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
    list_clip = UIPanel_GetLoadMenuListClipRect(ui);
    set_dir_button = UIPanel_GetLoadMenuSetDirectoryButtonRect(ui);
    TEST_ASSERT(set_dir_button.w > 0 && set_dir_button.h > 0);
    TEST_ASSERT(set_dir_button.x >= browser_rect.x);
    TEST_ASSERT(set_dir_button.x + set_dir_button.w <= browser_rect.x + browser_rect.w);
    TEST_ASSERT(set_dir_button.y >= browser_rect.y);
    TEST_ASSERT(set_dir_button.y + set_dir_button.h <= list_clip.y);

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

static bool test_stl_browser_action_hint_surfaces_import_failure(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    char temp_root[LINE_DRAWING_PATH_CAP];
    char stl_path[LINE_DRAWING_PATH_CAP];
    char action_text[320];

    ld_test_remove_file_browser_runtime_state();
    snprintf(temp_root, sizeof(temp_root), "/tmp/ld_stl_failure_hint_%u", (unsigned)SDL_GetTicks());
    TEST_ASSERT(mkdir(temp_root, 0755) == 0 || errno == EEXIST);
    snprintf(stl_path, sizeof(stl_path), "%s/sample.stl", temp_root);
    TEST_ASSERT(ld_test_write_tetrahedron_stl(stl_path));

    ld_test_init_runtime();
    state = Global_Get();
    ui = UIPanel_Get();
    TEST_ASSERT(state != NULL);
    TEST_ASSERT(ui != NULL);
    TEST_ASSERT(Global_SetObjectAssetRoot(temp_root, false));
    UIPanel_ActivateStlImportBrowser();
    snprintf(state->objectRuntimeMeshStatus,
             sizeof(state->objectRuntimeMeshStatus),
             "STL import failed: runtime mesh triangle is degenerate");
    TEST_ASSERT(UIPanel_GetFileBrowserActionHintText(ui, action_text, sizeof(action_text)));
    TEST_ASSERT(strstr(action_text, "runtime mesh triangle is degenerate") != NULL);
    ld_test_shutdown_runtime();

    ld_test_remove_file_browser_runtime_state();
    (void)unlink(stl_path);
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

static bool test_runtime_mesh_browser_places_scene_asset_instance(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    const Object3D* object = NULL;
    char asset_root[LINE_DRAWING_PATH_CAP];
    char nested_root[LINE_DRAWING_PATH_CAP];
    char alpha_path[LINE_DRAWING_PATH_CAP];
    char beta_path[LINE_DRAWING_PATH_CAP];
    char ignored_path[LINE_DRAWING_PATH_CAP];
    SDL_Rect list_clip = {0};
    int alpha_index = -1;
    int beta_index = -1;

    ld_test_remove_file_browser_runtime_state();
    snprintf(asset_root, sizeof(asset_root), "/tmp/ld_runtime_mesh_browser_%u", (unsigned)SDL_GetTicks());
    snprintf(nested_root, sizeof(nested_root), "%s/group", asset_root);
    TEST_ASSERT(mkdir(asset_root, 0755) == 0 || errno == EEXIST);
    TEST_ASSERT(mkdir(nested_root, 0755) == 0 || errno == EEXIST);
    snprintf(alpha_path, sizeof(alpha_path), "%s/alpha.runtime.json", asset_root);
    snprintf(beta_path, sizeof(beta_path), "%s/beta.runtime.json", nested_root);
    snprintf(ignored_path, sizeof(ignored_path), "%s/plain_asset.json", asset_root);

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);
    TEST_ASSERT(Global_SetWorkspaceMode(LINE_DRAWING_WORKSPACE_MODE_SCENE));
    TEST_ASSERT(ld_test_write_runtime_mesh_sidecar(alpha_path, "alpha_mesh"));
    TEST_ASSERT(ld_test_write_runtime_mesh_sidecar(beta_path, "beta_mesh"));
    TEST_ASSERT(ld_test_write_text_file_basic(ignored_path, "{\"schema\":\"object_asset_v1\"}\n"));
    TEST_ASSERT(Global_SetObjectAssetRoot(asset_root, false));

    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    ui->activeLeftTab = UI_PANEL_LEFT_TAB_FILE;
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);

    UIPanel_ActivateRuntimeMeshBrowser();
    TEST_ASSERT(UIPanel_IsLoadMenuOpen());
    TEST_ASSERT(ui->loadMenu.mode == UI_LOAD_MENU_MODE_RUNTIME_MESH);
    TEST_ASSERT(strcmp(ui->loadMenu.rootPath, asset_root) == 0);
    TEST_ASSERT(ui->loadMenu.count == 2);
    alpha_index = ld_test_find_load_menu_index(ui, "alpha.runtime.json");
    beta_index = ld_test_find_load_menu_index(ui, "group/beta.runtime.json");
    TEST_ASSERT(alpha_index >= 0);
    TEST_ASSERT(beta_index >= 0);
    TEST_ASSERT(ld_test_find_load_menu_index(ui, "plain_asset.json") < 0);

    list_clip = UIPanel_GetLoadMenuListClipRect(ui);
    TEST_ASSERT(list_clip.w > 0 && list_clip.h >= 24);
    ui->loadMenu.scrollOffsetPx = 0.0f;
    TEST_ASSERT(UIPanel_HandleLoadMenuClick(list_clip.x + 4,
                                            list_clip.y + (alpha_index * 24) + 12));

    TEST_ASSERT(strcmp(state->lastObjectRuntimeMeshPath, alpha_path) == 0);
    TEST_ASSERT(strstr(state->objectRuntimeMeshStatus, "Mesh placed") != NULL);
    TEST_ASSERT(Layout_ObjectStore_LiveCount(&state->layout.objectStore) == 1u);
    TEST_ASSERT(state->editor.selectedObject3DId != 0u);
    object = Layout_ObjectStore_FindConst(&state->layout.objectStore,
                                          state->editor.selectedObject3DId);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(object->kind == OBJECT3D_KIND_MESH_ASSET_INSTANCE);
    TEST_ASSERT(strcmp(object->meshInstance.assetId, "alpha_mesh") == 0);
    TEST_ASSERT(strcmp(object->meshInstance.runtimePath, alpha_path) == 0);
    TEST_ASSERT(ui->loadMenu.activeIndex == alpha_index);

    ld_test_shutdown_runtime();
    ld_test_remove_file_browser_runtime_state();
    (void)unlink(alpha_path);
    (void)unlink(beta_path);
    (void)unlink(ignored_path);
    (void)rmdir(nested_root);
    (void)rmdir(asset_root);
    return true;
}

static bool test_stl_import_browser_imports_and_places_scene_asset_instance(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    Object3D const* object = NULL;
    char stl_root[LINE_DRAWING_PATH_CAP];
    char stl_path[LINE_DRAWING_PATH_CAP];
    char authoring_path[LINE_DRAWING_PATH_CAP];
    char runtime_path[LINE_DRAWING_PATH_CAP];
    char preview_path[LINE_DRAWING_PATH_CAP];
    LayoutMeshRuntimePreviewStats preview_stats = {0};
    int stl_index = -1;
    SDL_Rect list_clip = {0};

    ld_test_remove_file_browser_runtime_state();
    snprintf(stl_root, sizeof(stl_root), "/tmp/ld_stl_import_browser_%u", (unsigned)SDL_GetTicks());
    TEST_ASSERT(mkdir(stl_root, 0755) == 0 || errno == EEXIST);
    snprintf(stl_path, sizeof(stl_path), "%s/tetra sample.stl", stl_root);
    snprintf(authoring_path, sizeof(authoring_path), "%s/imported_tetra_sample.json", stl_root);
    snprintf(runtime_path, sizeof(runtime_path), "%s/imported_tetra_sample.runtime.json", stl_root);
    snprintf(preview_path, sizeof(preview_path), "%s/imported_tetra_sample.preview.json", stl_root);
    TEST_ASSERT(ld_test_write_tetrahedron_stl(stl_path));

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);
    TEST_ASSERT(Global_SetObjectAssetRoot(stl_root, false));
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    ui->activeLeftTab = UI_PANEL_LEFT_TAB_FILE;
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);

    UIPanel_ActivateStlImportBrowser();
    TEST_ASSERT(UIPanel_IsLoadMenuOpen());
    TEST_ASSERT(ui->loadMenu.mode == UI_LOAD_MENU_MODE_STL_IMPORT);
    TEST_ASSERT(strcmp(ui->loadMenu.rootPath, stl_root) == 0);
    TEST_ASSERT(ui->loadMenu.count == 1);
    stl_index = ld_test_find_load_menu_index(ui, "tetra sample.stl");
    TEST_ASSERT(stl_index >= 0);

    list_clip = UIPanel_GetLoadMenuListClipRect(ui);
    TEST_ASSERT(list_clip.w > 0 && list_clip.h >= 24);
    ui->loadMenu.scrollOffsetPx = 0.0f;
    TEST_ASSERT(UIPanel_HandleLoadMenuClick(list_clip.x + 4,
                                            list_clip.y + (stl_index * 24) + 12));

    TEST_ASSERT(LineDrawingFileCatalog_PathIsRegularFile(authoring_path));
    TEST_ASSERT(LineDrawingFileCatalog_PathIsRegularFile(runtime_path));
    TEST_ASSERT(LineDrawingFileCatalog_PathIsRegularFile(preview_path));
    TEST_ASSERT(strcmp(state->lastObjectRuntimeMeshPath, runtime_path) == 0);
    TEST_ASSERT(strstr(state->objectRuntimeMeshStatus, "STL imported") != NULL);
    TEST_ASSERT(Layout_ObjectStore_LiveCount(&state->layout.objectStore) == 1u);
    TEST_ASSERT(state->editor.selectedObject3DId != 0u);
    object = Layout_ObjectStore_FindConst(&state->layout.objectStore,
                                          state->editor.selectedObject3DId);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(object->kind == OBJECT3D_KIND_MESH_ASSET_INSTANCE);
    TEST_ASSERT(strcmp(object->meshInstance.assetId, "imported_tetra_sample") == 0);
    TEST_ASSERT(strcmp(object->meshInstance.runtimePath, runtime_path) == 0);
    TEST_ASSERT(object->meshInstance.triangleCount == 4u);
    TEST_ASSERT(Layout_MeshRuntimePreview_LoadStats(runtime_path,
                                                    &preview_stats,
                                                    NULL,
                                                    0u));
    TEST_ASSERT(preview_stats.sourceTriangleCount == 4u);
    TEST_ASSERT(preview_stats.edgeCount == 6u);

    ld_test_shutdown_runtime();
    ld_test_remove_file_browser_runtime_state();
    (void)unlink(stl_path);
    (void)unlink(authoring_path);
    (void)unlink(runtime_path);
    (void)unlink(preview_path);
    (void)rmdir(stl_root);
    return true;
}

static bool test_stl_import_capacity_writes_bounded_preview_sidecar(void) {
    const uint32_t triangle_count = 90000u;
    char stl_root[LINE_DRAWING_PATH_CAP];
    char stl_path[LINE_DRAWING_PATH_CAP];
    char authoring_path[LINE_DRAWING_PATH_CAP];
    char runtime_path[LINE_DRAWING_PATH_CAP];
    char preview_path[LINE_DRAWING_PATH_CAP];
    char diagnostics[256];
    LayoutMeshRuntimePreviewStats preview_stats = {0};
    struct stat runtime_stat;
    struct stat preview_stat;

    snprintf(stl_root, sizeof(stl_root), "/tmp/ld_stl_capacity_%u", (unsigned)SDL_GetTicks());
    TEST_ASSERT(mkdir(stl_root, 0755) == 0 || errno == EEXIST);
    snprintf(stl_path, sizeof(stl_path), "%s/large repeated.stl", stl_root);
    snprintf(preview_path, sizeof(preview_path), "%s/imported_large_repeated.preview.json", stl_root);
    TEST_ASSERT(ld_test_write_repeated_triangle_binary_stl(stl_path, triangle_count));

    TEST_ASSERT(LayoutImportedMeshAsset_ImportStlToRuntime(stl_path,
                                                           stl_root,
                                                           authoring_path,
                                                           sizeof(authoring_path),
                                                           runtime_path,
                                                           sizeof(runtime_path),
                                                           diagnostics,
                                                           sizeof(diagnostics)));
    TEST_ASSERT(LineDrawingFileCatalog_PathIsRegularFile(authoring_path));
    TEST_ASSERT(LineDrawingFileCatalog_PathIsRegularFile(runtime_path));
    TEST_ASSERT(LineDrawingFileCatalog_PathIsRegularFile(preview_path));
    TEST_ASSERT(stat(runtime_path, &runtime_stat) == 0);
    TEST_ASSERT(stat(preview_path, &preview_stat) == 0);
    TEST_ASSERT(runtime_stat.st_size > (off_t)(4u * 1024u * 1024u));
    TEST_ASSERT(preview_stat.st_size < runtime_stat.st_size);
    TEST_ASSERT(Layout_MeshRuntimePreview_LoadStats(runtime_path,
                                                    &preview_stats,
                                                    diagnostics,
                                                    sizeof(diagnostics)));
    TEST_ASSERT(preview_stats.sourceTriangleCount == triangle_count);
    TEST_ASSERT(preview_stats.sampledTriangleCount < preview_stats.sourceTriangleCount);
    TEST_ASSERT(preview_stats.edgeCount <= LD_MESH_PREVIEW_MAX_EDGES);

    (void)unlink(stl_path);
    (void)unlink(authoring_path);
    (void)unlink(runtime_path);
    (void)unlink(preview_path);
    (void)rmdir(stl_root);
    return true;
}

static bool test_stl_import_auto_scales_and_fits_scene_bounds(void) {
    GlobalState* state = NULL;
    const Object3D* object = NULL;
    Vec3 world_min = {0};
    Vec3 world_max = {0};
    char stl_root[LINE_DRAWING_PATH_CAP];
    char stl_path[LINE_DRAWING_PATH_CAP];
    char authoring_path[LINE_DRAWING_PATH_CAP];
    char runtime_path[LINE_DRAWING_PATH_CAP];
    char preview_path[LINE_DRAWING_PATH_CAP];

    ld_test_remove_file_browser_runtime_state();
    snprintf(stl_root, sizeof(stl_root), "/tmp/ld_stl_autoscale_%u", (unsigned)SDL_GetTicks());
    TEST_ASSERT(mkdir(stl_root, 0755) == 0 || errno == EEXIST);
    snprintf(stl_path, sizeof(stl_path), "%s/oversized skull-ish.stl", stl_root);
    snprintf(authoring_path, sizeof(authoring_path), "%s/imported_oversized_skull_ish.json", stl_root);
    snprintf(runtime_path, sizeof(runtime_path), "%s/imported_oversized_skull_ish.runtime.json", stl_root);
    snprintf(preview_path, sizeof(preview_path), "%s/imported_oversized_skull_ish.preview.json", stl_root);
    TEST_ASSERT(ld_test_write_oversized_tetrahedron_stl(stl_path));

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);
    TEST_ASSERT(Global_SetObjectAssetRoot(stl_root, false));
    TEST_ASSERT(UIPanel_ImportStlAndPlaceFromPath(stl_path));
    TEST_ASSERT(LineDrawingFileCatalog_PathIsRegularFile(authoring_path));
    TEST_ASSERT(LineDrawingFileCatalog_PathIsRegularFile(runtime_path));
    TEST_ASSERT(LineDrawingFileCatalog_PathIsRegularFile(preview_path));
    TEST_ASSERT(state->editor.selectedObject3DId != 0u);
    object = Layout_ObjectStore_FindConst(&state->layout.objectStore,
                                          state->editor.selectedObject3DId);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(object->kind == OBJECT3D_KIND_MESH_ASSET_INSTANCE);
    TEST_ASSERT(object->transform.scale.x < 1.0f);
    TEST_ASSERT(ld_test_nearly_equal(object->transform.scale.x, object->transform.scale.y));
    TEST_ASSERT(ld_test_nearly_equal(object->transform.scale.x, object->transform.scale.z));
    TEST_ASSERT(Layout_Object3D_ComputeWorldAABB(object, &world_min, &world_max));
    TEST_ASSERT((world_max.x - world_min.x) <= 48.1f);
    TEST_ASSERT((world_max.y - world_min.y) <= 48.1f);
    TEST_ASSERT((world_max.z - world_min.z) <= 48.1f);
    TEST_ASSERT(state->layout.scene3d.bounds.enabled);
    TEST_ASSERT(ld_test_nearly_equal(state->layout.scene3d.bounds.min.x, world_min.x - 4.0f));
    TEST_ASSERT(ld_test_nearly_equal(state->layout.scene3d.bounds.max.x, world_max.x + 4.0f));
    TEST_ASSERT(ld_test_nearly_equal(state->layout.scene3d.bounds.min.y, world_min.y - 4.0f));
    TEST_ASSERT(ld_test_nearly_equal(state->layout.scene3d.bounds.max.y, world_max.y + 4.0f));
    TEST_ASSERT(ld_test_nearly_equal(state->layout.scene3d.bounds.min.z, world_min.z - 4.0f));
    TEST_ASSERT(ld_test_nearly_equal(state->layout.scene3d.bounds.max.z, world_max.z + 4.0f));

    ld_test_shutdown_runtime();
    ld_test_remove_file_browser_runtime_state();
    (void)unlink(stl_path);
    (void)unlink(authoring_path);
    (void)unlink(runtime_path);
    (void)unlink(preview_path);
    (void)rmdir(stl_root);
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
        { "session_input_root_change_preserves_loaded_layout_identity",
          test_session_input_root_change_preserves_loaded_layout_identity },
        { "session_input_root_change_updates_default_layout_path_before_load",
          test_session_input_root_change_updates_default_layout_path_before_load },
        { "file_browser_status_text_marks_active_session_row",
          test_file_browser_status_text_marks_active_session_row },
        { "stl_browser_action_hint_surfaces_import_failure",
          test_stl_browser_action_hint_surfaces_import_failure },
        { "file_browser_status_text_marks_remembered_fallback_row",
          test_file_browser_status_text_marks_remembered_fallback_row },
        { "file_browser_status_text_marks_unmatched_root_state",
          test_file_browser_status_text_marks_unmatched_root_state },
        { "file_browser_use_active_realigns_browser_root_and_highlight",
          test_file_browser_use_active_realigns_browser_root_and_highlight },
        { "file_browser_clear_last_removes_remembered_fallback",
          test_file_browser_clear_last_removes_remembered_fallback },
        { "runtime_mesh_browser_places_scene_asset_instance",
          test_runtime_mesh_browser_places_scene_asset_instance },
        { "stl_import_browser_imports_and_places_scene_asset_instance",
          test_stl_import_browser_imports_and_places_scene_asset_instance },
        { "stl_import_capacity_writes_bounded_preview_sidecar",
          test_stl_import_capacity_writes_bounded_preview_sidecar },
        { "stl_import_auto_scales_and_fits_scene_bounds",
          test_stl_import_auto_scales_and_fits_scene_bounds },
    };
    return run_test_cases("UIPanelFileBrowser", cases, sizeof(cases) / sizeof(cases[0]));
}
