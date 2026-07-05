#include "test_layout_internal.h"
#include "test_artifact_helpers.h"

#include "UI/ui_panel.h"
#include "UI/ui_panel_internal.h"
#include "UI/input_ui_panel.h"
#include "UI/ui_panel_file_layout.h"
#include "UI/panel/ui_panel_file_browser_internal.h"
#include "Core/line_drawing_file_catalog.h"
#include "Layout/asset/layout_imported_mesh_asset.h"
#include "Layout/scene/layout_mesh_preview_sidecar.h"
#include "Layout/scene/layout_mesh_runtime_preview.h"
#include "core_mesh_preview.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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

static const UIButton* ld_test_find_button_by_id(const UIPanelState* ui, int button_id) {
    if (!ui) return NULL;
    for (int i = 0; i < ui->count; ++i) {
        if (ui->buttons[i].id == button_id) return &ui->buttons[i];
    }
    return NULL;
}

static bool ld_test_tick_until_stl_import_idle(UIPanelState* ui, Uint32 timeout_ms) {
    const Uint32 start = SDL_GetTicks();
    if (!ui) return false;
    while (ui->loadMenu.asyncStlActive &&
           (Uint32)(SDL_GetTicks() - start) < timeout_ms) {
        UIPanel_TickLoadProgress();
        SDL_Delay(1);
    }
    UIPanel_TickLoadProgress();
    return !ui->loadMenu.asyncStlActive;
}

static bool ld_test_write_scene_contract(const char* scene_dir) {
    return ld_test_artifact_write_scene_contract(scene_dir,
                                                 "{\"schema\":\"scene_authoring_v1\"}\n",
                                                 "{\"schema\":\"scene_runtime_v1\"}\n");
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

static bool test_file_pane_action_buttons_use_equal_column_widths(void) {
    const int rows[][2] = {
        { UI_BTN_SAVE_JSON, UI_BTN_LOAD_JSON },
        { UI_BTN_LOAD_SCENE, UI_BTN_LOAD_STL },
        { UI_BTN_LOAD_MESH_ASSET, UI_BTN_EXPORT_SHAPE },
        { UI_BTN_EXPORT_SCENE, UI_BTN_FILE_BROWSER_USE_ACTIVE },
        { UI_BTN_INPUT_ROOT_EDIT, UI_BTN_INPUT_ROOT_FOLDER },
        { UI_BTN_OUTPUT_ROOT_EDIT, UI_BTN_OUTPUT_ROOT_FOLDER }
    };
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;

    ld_test_remove_file_browser_runtime_state();
    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    ui->activeLeftTab = UI_PANEL_LEFT_TAB_FILE;
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);
    UIPanel_ActivateJsonBrowser();

    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i) {
        const UIButton* left = ld_test_find_button_by_id(ui, rows[i][0]);
        const UIButton* right = ld_test_find_button_by_id(ui, rows[i][1]);
        TEST_ASSERT(left != NULL);
        TEST_ASSERT(right != NULL);
        TEST_ASSERT(left->bounds.w == right->bounds.w);
    }

    ld_test_shutdown_runtime();
    ld_test_remove_file_browser_runtime_state();
    return true;
}

static bool test_file_browser_close_api_synchronizes_open_and_visible(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;

    ld_test_remove_file_browser_runtime_state();
    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);

    ui->activeLeftTab = UI_PANEL_LEFT_TAB_FILE;
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);
    UIPanel_ActivateJsonBrowser();
    TEST_ASSERT(ui->loadMenu.open);
    TEST_ASSERT(ui->loadMenu.visible);
    TEST_ASSERT(UIPanel_IsLoadMenuOpen());

    UIPanel_CloseFileBrowser(ui);
    TEST_ASSERT(!ui->loadMenu.open);
    TEST_ASSERT(!ui->loadMenu.visible);
    TEST_ASSERT(!UIPanel_IsLoadMenuOpen());

    UIPanel_SetFileBrowserVisible(ui, true);
    TEST_ASSERT(ui->loadMenu.open);
    TEST_ASSERT(ui->loadMenu.visible);
    TEST_ASSERT(UIPanel_IsLoadMenuOpen());

    UIPanel_ToggleLoadMenu();
    TEST_ASSERT(!ui->loadMenu.open);
    TEST_ASSERT(!ui->loadMenu.visible);
    TEST_ASSERT(!UIPanel_IsLoadMenuOpen());

    ld_test_shutdown_runtime();
    ld_test_remove_file_browser_runtime_state();
    return true;
}

static bool test_file_pane_export_scene_button_click_exports_and_sets_status(void) {
    char root_template[] = "/tmp/ld_file_pane_export_click_XXXXXX";
    char* root = NULL;
    char expected_scene_dir[LINE_DRAWING_PATH_CAP];
    char expected_authoring[LINE_DRAWING_PATH_CAP];
    char expected_runtime[LINE_DRAWING_PATH_CAP];
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    const UIButton* export_button = NULL;
    int click_x = 0;
    int click_y = 0;

    root = ld_test_artifact_make_temp_dir(root_template);
    TEST_ASSERT(root != NULL);

    ld_test_remove_file_browser_runtime_state();
    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    TEST_ASSERT(Global_SetOutputRoot(root, false));
    Global_OnLayoutLoaded("/tmp/ld_file_pane_export_click/source_layout.json");
    TEST_ASSERT(Layout_AddAnchor3(&state->layout, (Vec3){0.0f, 0.0f, 0.0f}) >= 0);
    TEST_ASSERT(Layout_AddAnchor3(&state->layout, (Vec3){1.0f, 0.0f, 1.0f}) >= 0);
    Layout_AddWall3(&state->layout, (Vec3){0.0f, 0.0f, 0.0f}, (Vec3){1.0f, 0.0f, 1.0f});

    ui->activeLeftTab = UI_PANEL_LEFT_TAB_FILE;
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);
    UIPanel_ActivateJsonBrowser();
    TEST_ASSERT(ui->loadMenu.open);
    TEST_ASSERT(ui->loadMenu.visible);
    export_button = ld_test_find_button_by_id(ui, UI_BTN_EXPORT_SCENE);
    TEST_ASSERT(export_button != NULL);
    TEST_ASSERT(export_button->bounds.w > 0 && export_button->bounds.h > 0);
    click_x = export_button->bounds.x + export_button->bounds.w / 2;
    click_y = export_button->bounds.y + export_button->bounds.h / 2;

    TEST_ASSERT(UIPanel_HandleClick(click_x, click_y));
    TEST_ASSERT(!ui->loadMenu.open);
    TEST_ASSERT(!ui->loadMenu.visible);
    TEST_ASSERT(!UIPanel_IsLoadMenuOpen());

    TEST_ASSERT(snprintf(expected_scene_dir,
                         sizeof(expected_scene_dir),
                         "%s/source_layout",
                         root) < (int)sizeof(expected_scene_dir));
    TEST_ASSERT(snprintf(expected_authoring,
                         sizeof(expected_authoring),
                         "%s/scene_authoring.json",
                         expected_scene_dir) < (int)sizeof(expected_authoring));
    TEST_ASSERT(snprintf(expected_runtime,
                         sizeof(expected_runtime),
                         "%s/scene_runtime.json",
                         expected_scene_dir) < (int)sizeof(expected_runtime));
    TEST_ASSERT(access(expected_authoring, F_OK) == 0);
    TEST_ASSERT(access(expected_runtime, F_OK) == 0);
    TEST_ASSERT(strncmp(ui->filePane.actionStatus, "Export Scene OK", 15) == 0);
    TEST_ASSERT(UIPanel_FilePaneActionStatusIsLive(ui));
    ui->filePane.actionStatusSetTicks = SDL_GetTicks() - 100000u;
    TEST_ASSERT(!UIPanel_FilePaneActionStatusIsLive(ui));
    TEST_ASSERT(export_button->pressed);

    ld_test_shutdown_runtime();
    ld_test_remove_file_browser_runtime_state();
    (void)unlink(expected_authoring);
    (void)unlink(expected_runtime);
    (void)rmdir(expected_scene_dir);
    (void)rmdir(root);
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
    TEST_ASSERT(Global_SetObjectRuntimeMeshStatus(
        "STL import failed: runtime mesh triangle is degenerate"));
    TEST_ASSERT(UIPanel_GetFileBrowserActionHintText(ui, action_text, sizeof(action_text)));
    TEST_ASSERT(strstr(action_text, "runtime mesh triangle is degenerate") != NULL);
    ld_test_shutdown_runtime();

    ld_test_remove_file_browser_runtime_state();
    (void)unlink(stl_path);
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

    TEST_ASSERT(strcmp(Global_GetLastObjectRuntimeMeshPath(), alpha_path) == 0);
    TEST_ASSERT(strstr(Global_GetObjectRuntimeMeshStatus(), "Mesh placed") != NULL);
    TEST_ASSERT(Layout_ObjectStore_LiveCount(&state->layout.objectStore) == 1u);
    TEST_ASSERT(state->editor.selectedObject3DId != 0u);
    object = Layout_ObjectStore_FindConst(&state->layout.objectStore,
                                          state->editor.selectedObject3DId);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(object->kind == OBJECT3D_KIND_MESH_ASSET_INSTANCE);
    TEST_ASSERT(strcmp(object->meshInstance.assetId, "alpha_mesh") == 0);
    TEST_ASSERT(strcmp(object->meshInstance.runtimePath, alpha_path) == 0);
    TEST_ASSERT(ui->loadMenu.activeIndex == alpha_index);
    TEST_ASSERT(ui->loadMenu.loadProgressState == UI_LOAD_PROGRESS_COMPLETE);
    TEST_ASSERT(ui->loadMenu.loadProgressMode == UI_LOAD_MENU_MODE_RUNTIME_MESH);
    TEST_ASSERT(strcmp(ui->loadMenu.loadProgressPath, alpha_path) == 0);
    TEST_ASSERT(ui->loadMenu.loadProgressPermille == 1000);
    TEST_ASSERT(UIPanel_FindLoadProgressIndex(ui) == alpha_index);
    TEST_ASSERT(UIPanel_LoadMenuRowHeight(ui, alpha_index) > 24);

    ld_test_shutdown_runtime();
    ld_test_remove_file_browser_runtime_state();
    (void)unlink(alpha_path);
    (void)unlink(beta_path);
    (void)unlink(ignored_path);
    (void)rmdir(nested_root);
    (void)rmdir(asset_root);
    return true;
}

static bool test_stl_catalog_recurses_into_curated_source_directories(void) {
    char stl_root[LINE_DRAWING_PATH_CAP];
    char curated_dir[LINE_DRAWING_PATH_CAP];
    char asset_dir[LINE_DRAWING_PATH_CAP];
    char source_dir[LINE_DRAWING_PATH_CAP];
    char stl_path[LINE_DRAWING_PATH_CAP];
    LineDrawingFileCatalogEntry entries[8];
    int count = 0;

    snprintf(stl_root, sizeof(stl_root), "/tmp/ld_stl_recursive_catalog_%u", (unsigned)SDL_GetTicks());
    snprintf(curated_dir, sizeof(curated_dir), "%s/curated", stl_root);
    snprintf(asset_dir, sizeof(asset_dir), "%s/generated_table", curated_dir);
    snprintf(source_dir, sizeof(source_dir), "%s/source", asset_dir);
    snprintf(stl_path, sizeof(stl_path), "%s/generated_table.stl", source_dir);

    TEST_ASSERT(mkdir(stl_root, 0755) == 0 || errno == EEXIST);
    TEST_ASSERT(mkdir(curated_dir, 0755) == 0 || errno == EEXIST);
    TEST_ASSERT(mkdir(asset_dir, 0755) == 0 || errno == EEXIST);
    TEST_ASSERT(mkdir(source_dir, 0755) == 0 || errno == EEXIST);
    TEST_ASSERT(ld_test_write_tetrahedron_stl(stl_path));

    count = LineDrawingFileCatalog_ScanStlEntries(entries, 8, stl_root);
    TEST_ASSERT(count == 1);
    TEST_ASSERT(strcmp(entries[0].label, "curated/generated_table/source/generated_table.stl") == 0);
    TEST_ASSERT(strcmp(entries[0].path, stl_path) == 0);

    (void)unlink(stl_path);
    (void)rmdir(source_dir);
    (void)rmdir(asset_dir);
    (void)rmdir(curated_dir);
    (void)rmdir(stl_root);
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
    TEST_ASSERT(ui->loadMenu.loadProgressState == UI_LOAD_PROGRESS_LOADING);
    TEST_ASSERT(ui->loadMenu.asyncStlActive);
    TEST_ASSERT(UIPanel_FindLoadProgressIndex(ui) == stl_index);
    TEST_ASSERT(UIPanel_LoadMenuContentHeight(ui) > 24.0f);
    TEST_ASSERT(ld_test_tick_until_stl_import_idle(ui, 5000u));

    TEST_ASSERT(LineDrawingFileCatalog_PathIsRegularFile(authoring_path));
    TEST_ASSERT(LineDrawingFileCatalog_PathIsRegularFile(runtime_path));
    TEST_ASSERT(LineDrawingFileCatalog_PathIsRegularFile(preview_path));
    TEST_ASSERT(strcmp(Global_GetLastObjectRuntimeMeshPath(), runtime_path) == 0);
    TEST_ASSERT(strstr(Global_GetObjectRuntimeMeshStatus(), "STL imported") != NULL);
    TEST_ASSERT(Layout_ObjectStore_LiveCount(&state->layout.objectStore) == 1u);
    TEST_ASSERT(state->editor.selectedObject3DId != 0u);
    object = Layout_ObjectStore_FindConst(&state->layout.objectStore,
                                          state->editor.selectedObject3DId);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(object->kind == OBJECT3D_KIND_MESH_ASSET_INSTANCE);
    TEST_ASSERT(strcmp(object->meshInstance.assetId, "imported_tetra_sample") == 0);
    TEST_ASSERT(strcmp(object->meshInstance.runtimePath, runtime_path) == 0);
    TEST_ASSERT(object->meshInstance.triangleCount == 4u);
    TEST_ASSERT(ld_test_nearly_equal(object->transform.position.x, 0.0f));
    TEST_ASSERT(ld_test_nearly_equal(object->transform.position.y, 0.0f));
    TEST_ASSERT(ld_test_nearly_equal(object->transform.position.z, 0.0f));
    TEST_ASSERT(object->transform.scale.x > 1.0f);
    TEST_ASSERT(ld_test_nearly_equal(object->transform.scale.x, object->transform.scale.y));
    TEST_ASSERT(ld_test_nearly_equal(object->transform.scale.x, object->transform.scale.z));
    TEST_ASSERT(Layout_MeshRuntimePreview_LoadStats(runtime_path,
                                                    &preview_stats,
                                                    NULL,
                                                    0u));
    TEST_ASSERT(preview_stats.sourceTriangleCount == 4u);
    TEST_ASSERT(preview_stats.edgeCount == 6u);
    TEST_ASSERT(ui->loadMenu.loadProgressState == UI_LOAD_PROGRESS_COMPLETE);
    TEST_ASSERT(ui->loadMenu.loadProgressMode == UI_LOAD_MENU_MODE_STL_IMPORT);
    TEST_ASSERT(strcmp(ui->loadMenu.loadProgressPath, stl_path) == 0);
    TEST_ASSERT(ui->loadMenu.loadProgressPermille == 1000);
    TEST_ASSERT(UIPanel_FindLoadProgressIndex(ui) == stl_index);
    TEST_ASSERT(UIPanel_LoadMenuContentHeight(ui) > 24.0f);

    TEST_ASSERT(UIPanel_HandleLoadMenuClick(list_clip.x + 4,
                                            list_clip.y + (stl_index * 24) + 12));
    TEST_ASSERT(!ui->loadMenu.asyncStlActive);
    TEST_ASSERT(ui->loadMenu.loadProgressState == UI_LOAD_PROGRESS_COMPLETE);
    TEST_ASSERT(strstr(ui->loadMenu.loadProgressDetail, "cached STL") != NULL);
    TEST_ASSERT(Layout_ObjectStore_LiveCount(&state->layout.objectStore) == 2u);
    TEST_ASSERT(strcmp(Global_GetLastObjectRuntimeMeshPath(), runtime_path) == 0);

    ui->loadMenu.loadProgressFinishedTicks = SDL_GetTicks() - 2000u;
    UIPanel_TickLoadProgress();
    TEST_ASSERT(ui->loadMenu.loadProgressState == UI_LOAD_PROGRESS_NONE);
    TEST_ASSERT(UIPanel_LoadMenuContentHeight(ui) == 24.0f);

    ld_test_shutdown_runtime();
    ld_test_remove_file_browser_runtime_state();
    (void)unlink(stl_path);
    (void)unlink(authoring_path);
    (void)unlink(runtime_path);
    (void)unlink(preview_path);
    (void)rmdir(stl_root);
    return true;
}

static bool test_stl_import_browser_reimports_stale_cache(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    char stl_root[LINE_DRAWING_PATH_CAP];
    char stl_path[LINE_DRAWING_PATH_CAP];
    char authoring_path[LINE_DRAWING_PATH_CAP];
    char runtime_path[LINE_DRAWING_PATH_CAP];
    char preview_path[LINE_DRAWING_PATH_CAP];
    int stl_index = -1;
    SDL_Rect list_clip = {0};

    ld_test_remove_file_browser_runtime_state();
    snprintf(stl_root, sizeof(stl_root), "/tmp/ld_stl_stale_cache_%u", (unsigned)SDL_GetTicks());
    TEST_ASSERT(mkdir(stl_root, 0755) == 0 || errno == EEXIST);
    snprintf(stl_path, sizeof(stl_path), "%s/tetra stale.stl", stl_root);
    snprintf(authoring_path, sizeof(authoring_path), "%s/imported_tetra_stale.json", stl_root);
    snprintf(runtime_path, sizeof(runtime_path), "%s/imported_tetra_stale.runtime.json", stl_root);
    snprintf(preview_path, sizeof(preview_path), "%s/imported_tetra_stale.preview.json", stl_root);
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
    stl_index = ld_test_find_load_menu_index(ui, "tetra stale.stl");
    TEST_ASSERT(stl_index >= 0);
    list_clip = UIPanel_GetLoadMenuListClipRect(ui);
    TEST_ASSERT(UIPanel_HandleLoadMenuClick(list_clip.x + 4,
                                            list_clip.y + (stl_index * 24) + 12));
    TEST_ASSERT(ld_test_tick_until_stl_import_idle(ui, 5000u));
    TEST_ASSERT(Layout_ObjectStore_LiveCount(&state->layout.objectStore) == 1u);
    TEST_ASSERT(LineDrawingFileCatalog_PathIsRegularFile(authoring_path));
    TEST_ASSERT(LineDrawingFileCatalog_PathIsRegularFile(runtime_path));
    TEST_ASSERT(LineDrawingFileCatalog_PathIsRegularFile(preview_path));

    SDL_Delay(1100);
    TEST_ASSERT(ld_test_write_oversized_tetrahedron_stl(stl_path));
    TEST_ASSERT(UIPanel_HandleLoadMenuClick(list_clip.x + 4,
                                            list_clip.y + (stl_index * 24) + 12));
    TEST_ASSERT(ui->loadMenu.asyncStlActive);
    TEST_ASSERT(ui->loadMenu.loadProgressState == UI_LOAD_PROGRESS_LOADING);
    TEST_ASSERT(ld_test_tick_until_stl_import_idle(ui, 5000u));
    TEST_ASSERT(Layout_ObjectStore_LiveCount(&state->layout.objectStore) == 2u);
    TEST_ASSERT(strstr(ui->loadMenu.loadProgressDetail, "Import STL finished") != NULL);

    ld_test_shutdown_runtime();
    ld_test_remove_file_browser_runtime_state();
    (void)unlink(stl_path);
    (void)unlink(authoring_path);
    (void)unlink(runtime_path);
    (void)unlink(preview_path);
    (void)rmdir(stl_root);
    return true;
}

static bool test_stl_import_failure_updates_progress_and_runtime_status(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    char stl_root[LINE_DRAWING_PATH_CAP];
    char stl_path[LINE_DRAWING_PATH_CAP];
    int stl_index = -1;
    SDL_Rect list_clip = {0};

    ld_test_remove_file_browser_runtime_state();
    snprintf(stl_root, sizeof(stl_root), "/tmp/ld_stl_failure_status_%u", (unsigned)SDL_GetTicks());
    TEST_ASSERT(mkdir(stl_root, 0755) == 0 || errno == EEXIST);
    snprintf(stl_path, sizeof(stl_path), "%s/not_really.stl", stl_root);
    TEST_ASSERT(ld_test_write_text_file_basic(stl_path, "not an stl\n"));

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);
    TEST_ASSERT(Global_SetObjectAssetRoot(stl_root, false));
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    ui->activeLeftTab = UI_PANEL_LEFT_TAB_FILE;
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);

    UIPanel_ActivateStlImportBrowser();
    stl_index = ld_test_find_load_menu_index(ui, "not_really.stl");
    TEST_ASSERT(stl_index >= 0);
    list_clip = UIPanel_GetLoadMenuListClipRect(ui);
    TEST_ASSERT(UIPanel_HandleLoadMenuClick(list_clip.x + 4,
                                            list_clip.y + (stl_index * 24) + 12));
    TEST_ASSERT(ld_test_tick_until_stl_import_idle(ui, 5000u));
    TEST_ASSERT(ui->loadMenu.loadProgressState == UI_LOAD_PROGRESS_FAILED);
    TEST_ASSERT(strstr(ui->loadMenu.loadProgressDetail, "STL import failed:") != NULL);
    TEST_ASSERT(strstr(Global_GetObjectRuntimeMeshStatus(), "STL import failed:") != NULL);
    TEST_ASSERT(strcmp(ui->loadMenu.loadProgressDetail,
                       Global_GetObjectRuntimeMeshStatus()) == 0);

    ld_test_shutdown_runtime();
    ld_test_remove_file_browser_runtime_state();
    (void)unlink(stl_path);
    (void)rmdir(stl_root);
    return true;
}

static bool test_stl_import_wrong_workspace_sets_failure_status(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    char stl_root[LINE_DRAWING_PATH_CAP];
    char stl_path[LINE_DRAWING_PATH_CAP];

    ld_test_remove_file_browser_runtime_state();
    snprintf(stl_root, sizeof(stl_root), "/tmp/ld_stl_wrong_workspace_%u", (unsigned)SDL_GetTicks());
    TEST_ASSERT(mkdir(stl_root, 0755) == 0 || errno == EEXIST);
    snprintf(stl_path, sizeof(stl_path), "%s/sample.stl", stl_root);
    TEST_ASSERT(ld_test_write_tetrahedron_stl(stl_path));

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);
    TEST_ASSERT(Global_SetObjectAssetRoot(stl_root, false));
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    TEST_ASSERT(Global_SetWorkspaceMode(LINE_DRAWING_WORKSPACE_MODE_OBJECT));

    TEST_ASSERT(!UIPanel_ImportStlAndPlaceFromPath(stl_path));
    TEST_ASSERT(strstr(Global_GetObjectRuntimeMeshStatus(), "switch to scene mode") != NULL);
    TEST_ASSERT(UIPanel_FilePaneActionStatusIsLive(ui));
    TEST_ASSERT(strstr(ui->filePane.actionStatus, "switch to scene mode") != NULL);

    ld_test_shutdown_runtime();
    ld_test_remove_file_browser_runtime_state();
    (void)unlink(stl_path);
    (void)rmdir(stl_root);
    return true;
}

static bool test_file_pane_failure_action_status_parity(void) {
    UIPanelState* ui = NULL;
    char missing_json[LINE_DRAWING_PATH_CAP];
    char missing_scene[LINE_DRAWING_PATH_CAP];

    ld_test_remove_file_browser_runtime_state();
    snprintf(missing_json, sizeof(missing_json), "/tmp/ld_missing_layout_%u.json", (unsigned)SDL_GetTicks());
    snprintf(missing_scene, sizeof(missing_scene), "/tmp/ld_missing_scene_%u.json", (unsigned)SDL_GetTicks());

    ld_test_init_runtime();
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);

    TEST_ASSERT(!UIPanel_LoadLayoutFromPath(missing_json));
    TEST_ASSERT(UIPanel_FilePaneActionStatusIsLive(ui));
    TEST_ASSERT(strstr(ui->filePane.actionStatus, "Load JSON failed") != NULL);

    UIPanel_SetFilePaneActionStatus("");
    TEST_ASSERT(!UIPanel_LoadSceneFromPath(missing_scene));
    TEST_ASSERT(UIPanel_FilePaneActionStatusIsLive(ui));
    TEST_ASSERT(strstr(ui->filePane.actionStatus, "Load scene failed") != NULL);

    UIPanel_SetFilePaneActionStatus("");
    TEST_ASSERT(!UIPanel_ExportObjectRuntimeMesh());
    TEST_ASSERT(UIPanel_FilePaneActionStatusIsLive(ui));
    TEST_ASSERT(strstr(ui->filePane.actionStatus, "Mesh export failed") != NULL);

    ld_test_shutdown_runtime();
    ld_test_remove_file_browser_runtime_state();
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
    TEST_ASSERT(preview_stats.previewEdgeCount == preview_stats.edgeCount);
    TEST_ASSERT(preview_stats.previewTriangleCount == 0u);
    TEST_ASSERT(preview_stats.previewEdgeCount < preview_stats.sourceTriangleCount);
    TEST_ASSERT(preview_stats.maxBudget == LD_MESH_PREVIEW_MAX_EDGES);
    TEST_ASSERT(preview_stats.coverageRatio > 0.0);
    TEST_ASSERT(preview_stats.coverageRatio <= 1.0);
    TEST_ASSERT(preview_stats.edgeCount <= LD_MESH_PREVIEW_MAX_EDGES);

    (void)unlink(stl_path);
    (void)unlink(authoring_path);
    (void)unlink(runtime_path);
    (void)unlink(preview_path);
    (void)rmdir(stl_root);
    return true;
}

static bool test_stl_import_auto_scales_and_preserves_scene_bounds(void) {
    GlobalState* state = NULL;
    const Object3D* object = NULL;
    Vec3 world_min = {0};
    Vec3 world_max = {0};
    Vec3 bounds_min_before = {0};
    Vec3 bounds_max_before = {0};
    bool bounds_enabled_before = false;
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
    bounds_enabled_before = state->layout.scene3d.bounds.enabled;
    bounds_min_before = state->layout.scene3d.bounds.min;
    bounds_max_before = state->layout.scene3d.bounds.max;
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
    TEST_ASSERT(ld_test_nearly_equal(object->transform.position.x, 0.0f));
    TEST_ASSERT(ld_test_nearly_equal(object->transform.position.y, 0.0f));
    TEST_ASSERT(ld_test_nearly_equal(object->transform.position.z, 0.0f));
    TEST_ASSERT(object->transform.scale.x < 1.0f);
    TEST_ASSERT(ld_test_nearly_equal(object->transform.scale.x, object->transform.scale.y));
    TEST_ASSERT(ld_test_nearly_equal(object->transform.scale.x, object->transform.scale.z));
    TEST_ASSERT(Layout_Object3D_ComputeWorldAABB(object, &world_min, &world_max));
    TEST_ASSERT((world_max.x - world_min.x) <= 48.1f);
    TEST_ASSERT((world_max.y - world_min.y) <= 48.1f);
    TEST_ASSERT((world_max.z - world_min.z) <= 48.1f);
    TEST_ASSERT(state->layout.scene3d.bounds.enabled == bounds_enabled_before);
    TEST_ASSERT(ld_test_nearly_equal(state->layout.scene3d.bounds.min.x, bounds_min_before.x));
    TEST_ASSERT(ld_test_nearly_equal(state->layout.scene3d.bounds.max.x, bounds_max_before.x));
    TEST_ASSERT(ld_test_nearly_equal(state->layout.scene3d.bounds.min.y, bounds_min_before.y));
    TEST_ASSERT(ld_test_nearly_equal(state->layout.scene3d.bounds.max.y, bounds_max_before.y));
    TEST_ASSERT(ld_test_nearly_equal(state->layout.scene3d.bounds.min.z, bounds_min_before.z));
    TEST_ASSERT(ld_test_nearly_equal(state->layout.scene3d.bounds.max.z, bounds_max_before.z));

    ld_test_shutdown_runtime();
    ld_test_remove_file_browser_runtime_state();
    (void)unlink(stl_path);
    (void)unlink(authoring_path);
    (void)unlink(runtime_path);
    (void)unlink(preview_path);
    (void)rmdir(stl_root);
    return true;
}

static bool test_stl_import_bounds_proxy_preview_keeps_mesh_selectable(void) {
    GlobalState* state = NULL;
    const Object3D* object = NULL;
    Hitbox hit = {0};
    SpaceViewContext view_ctx = {0};
    Vec3 center = {0};
    Vec2 center_screen = {0};
    char stl_root[LINE_DRAWING_PATH_CAP];
    char stl_path[LINE_DRAWING_PATH_CAP];
    char authoring_path[LINE_DRAWING_PATH_CAP];
    char runtime_path[LINE_DRAWING_PATH_CAP];
    char preview_path[LINE_DRAWING_PATH_CAP];
    char generated_preview_path[LINE_DRAWING_PATH_CAP];
    LayoutMeshRuntimePreviewStats preview_stats = {0};
    CoreResult preview_result = {0};

    ld_test_remove_file_browser_runtime_state();
    snprintf(stl_root, sizeof(stl_root), "/tmp/ld_stl_bounds_proxy_%u", (unsigned)SDL_GetTicks());
    TEST_ASSERT(mkdir(stl_root, 0755) == 0 || errno == EEXIST);
    snprintf(stl_path, sizeof(stl_path), "%s/degraded selectable.stl", stl_root);
    snprintf(authoring_path, sizeof(authoring_path), "%s/imported_degraded_selectable.json", stl_root);
    snprintf(runtime_path,
             sizeof(runtime_path),
             "%s/imported_degraded_selectable.runtime.json",
             stl_root);
    snprintf(preview_path,
             sizeof(preview_path),
             "%s/imported_degraded_selectable.preview.json",
             stl_root);
    TEST_ASSERT(ld_test_write_oversized_tetrahedron_stl(stl_path));

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);
    TEST_ASSERT(Global_SetObjectAssetRoot(stl_root, false));
    TEST_ASSERT(UIPanel_ImportStlAndPlaceFromPath(stl_path));
    TEST_ASSERT(LineDrawingFileCatalog_PathIsRegularFile(authoring_path));
    TEST_ASSERT(LineDrawingFileCatalog_PathIsRegularFile(runtime_path));
    TEST_ASSERT(LineDrawingFileCatalog_PathIsRegularFile(preview_path));

    preview_result = core_mesh_preview_save_for_runtime_file(
        runtime_path,
        CORE_MESH_PREVIEW_MODE_BOUNDS_PROXY_V1,
        0u,
        generated_preview_path,
        sizeof(generated_preview_path));
    TEST_ASSERT(preview_result.code == CORE_OK);
    TEST_ASSERT(strcmp(generated_preview_path, preview_path) == 0);
    TEST_ASSERT(Layout_MeshRuntimePreview_LoadStats(runtime_path,
                                                    &preview_stats,
                                                    NULL,
                                                    0u));
    TEST_ASSERT(strcmp(preview_stats.previewMode, "bounds_proxy_v1") == 0);
    TEST_ASSERT(preview_stats.metadataOnly);
    TEST_ASSERT(preview_stats.edgeCount == 0u);
    TEST_ASSERT(preview_stats.previewEdgeCount == 0u);
    TEST_ASSERT(preview_stats.maxSpan > 0.0);
    TEST_ASSERT(preview_stats.boundingSphereRadius > 0.0);

    TEST_ASSERT(state->editor.selectedObject3DId != 0u);
    object = Layout_ObjectStore_FindConst(&state->layout.objectStore,
                                          state->editor.selectedObject3DId);
    TEST_ASSERT(object != NULL);
    TEST_ASSERT(object->kind == OBJECT3D_KIND_MESH_ASSET_INSTANCE);
    TEST_ASSERT(Layout_Object3D_ComputeVisualCenter(object, &center));
    view_ctx = SpaceAdapter_BuildViewContext(state);
    center_screen = WorldToScreen(SpaceAdapter_ProjectToView(center, &view_ctx), &state->grid);
    Global_RebuildHitboxesIfDirty();
    hit = HitboxSystem_GetHitAtOfType((int)lroundf(center_screen.x),
                                      (int)lroundf(center_screen.y),
                                      HITBOX_OBJECT3D);
    TEST_ASSERT(hit.type == HITBOX_OBJECT3D);
    TEST_ASSERT(hit.index == (int)object->objectId);

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
        { "file_pane_action_buttons_use_equal_column_widths",
          test_file_pane_action_buttons_use_equal_column_widths },
        { "file_browser_close_api_synchronizes_open_and_visible",
          test_file_browser_close_api_synchronizes_open_and_visible },
        { "file_pane_export_scene_button_click_exports_and_sets_status",
          test_file_pane_export_scene_button_click_exports_and_sets_status },
        { "stl_browser_action_hint_surfaces_import_failure",
          test_stl_browser_action_hint_surfaces_import_failure },
        { "stl_catalog_recurses_into_curated_source_directories",
          test_stl_catalog_recurses_into_curated_source_directories },
        { "runtime_mesh_browser_places_scene_asset_instance",
          test_runtime_mesh_browser_places_scene_asset_instance },
        { "stl_import_browser_imports_and_places_scene_asset_instance",
          test_stl_import_browser_imports_and_places_scene_asset_instance },
        { "stl_import_browser_reimports_stale_cache",
          test_stl_import_browser_reimports_stale_cache },
        { "stl_import_failure_updates_progress_and_runtime_status",
          test_stl_import_failure_updates_progress_and_runtime_status },
        { "stl_import_wrong_workspace_sets_failure_status",
          test_stl_import_wrong_workspace_sets_failure_status },
        { "file_pane_failure_action_status_parity",
          test_file_pane_failure_action_status_parity },
        { "stl_import_capacity_writes_bounded_preview_sidecar",
          test_stl_import_capacity_writes_bounded_preview_sidecar },
        { "stl_import_auto_scales_and_preserves_scene_bounds",
          test_stl_import_auto_scales_and_preserves_scene_bounds },
        { "stl_import_bounds_proxy_preview_keeps_mesh_selectable",
          test_stl_import_bounds_proxy_preview_keeps_mesh_selectable },
    };
    return run_test_cases("UIPanelFileBrowser", cases, sizeof(cases) / sizeof(cases[0]));
}
