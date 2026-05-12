#include "test_layout_internal.h"
#include "Editor/primitive_placement_preview.h"
#include "UI/ui_panel_internal.h"
#include "Tools/scene_export.h"

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

static SDL_Rect ld_test_pane_rect_to_sdl(CorePaneRect rect) {
    SDL_Rect out = {0, 0, 0, 0};
    int x0 = (int)floorf(rect.x);
    int y0 = (int)floorf(rect.y);
    int x1 = (int)ceilf(rect.x + rect.width);
    int y1 = (int)ceilf(rect.y + rect.height);
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;
    out.x = x0;
    out.y = y0;
    out.w = x1 - x0;
    out.h = y1 - y0;
    return out;
}

static bool ld_test_make_dir(const char* path) {
    if (!path || !path[0]) return false;
    if (mkdir(path, 0755) == 0) return true;
    return errno == EEXIST;
}

static bool ld_test_write_text_file(const char* path, const char* text) {
    FILE* fp = NULL;
    size_t len = 0u;
    if (!path || !text) return false;
    fp = fopen(path, "wb");
    if (!fp) return false;
    len = strlen(text);
    if (fwrite(text, 1u, len, fp) != len) {
        fclose(fp);
        return false;
    }
    return fclose(fp) == 0;
}

static void ld_test_remove_file_if_exists(const char* path) {
    if (!path || !path[0]) return;
    (void)unlink(path);
}

static void ld_test_remove_dir_if_exists(const char* path) {
    if (!path || !path[0]) return;
    (void)rmdir(path);
}

static bool ld_test_write_layout_file(const char* path, float x_offset) {
    Layout layout;
    bool ok = false;
    if (!path || !path[0]) return false;
    Layout_Init(&layout, 1.0f);
    ok = Layout_AddAnchor3(&layout, (Vec3){x_offset, 0.0f, 0.0f}) >= 0 &&
         Layout_AddAnchor3(&layout, (Vec3){x_offset + 2.0f, 1.0f, 0.0f}) >= 0 &&
         Layout_SaveToFile(&layout, path);
    Layout_Free(&layout);
    return ok;
}

static int ld_test_find_load_menu_index(const UIPanelState* ui, const char* label) {
    if (!ui || !label) return -1;
    for (int i = 0; i < ui->loadMenu.count; ++i) {
        if (strcmp(ui->loadMenu.entries[i], label) == 0) return i;
    }
    return -1;
}

static void ld_test_load_menu_click_point(const UIPanelState* ui, int index, int* out_x, int* out_y) {
    SDL_Rect rect = UIPanel_GetLoadMenuRect(ui);
    const int header_h = 32;
    const int row_h = 26;
    if (out_x) *out_x = rect.x + 16;
    if (out_y) *out_y = rect.y + header_h + (index * row_h) + (row_h / 2);
}

static bool ld_test_write_scene_contract(const char* scene_dir) {
    char authoring_path[512];
    char runtime_path[512];
    if (!scene_dir || !scene_dir[0]) return false;
    if (!ld_test_make_dir(scene_dir)) return false;
    if (snprintf(authoring_path, sizeof(authoring_path), "%s/scene_authoring.json", scene_dir) >= (int)sizeof(authoring_path)) {
        return false;
    }
    if (snprintf(runtime_path, sizeof(runtime_path), "%s/scene_runtime.json", scene_dir) >= (int)sizeof(runtime_path)) {
        return false;
    }
    return ld_test_write_text_file(authoring_path, "{\n  \"scene_id\": \"test_authoring\"\n}\n") &&
           ld_test_write_text_file(runtime_path, "{\n  \"scene_id\": \"test_runtime\"\n}\n");
}

static bool ld_test_write_legacy_scene_contract(const char* scene_dir) {
    char authoring_path[512];
    char runtime_path[512];
    const char* authoring_json =
        "{\n"
        "  \"schema_family\": \"codework_scene\",\n"
        "  \"schema_variant\": \"scene_authoring_v1\",\n"
        "  \"schema_version\": 1,\n"
        "  \"scene_id\": \"scene_line_drawing_legacy_scene\",\n"
        "  \"objects\": [\n"
        "    {\n"
        "      \"object_id\": \"obj3d_7\",\n"
        "      \"object_type\": \"plane_primitive\",\n"
        "      \"dimensional_mode\": \"plane_locked\",\n"
        "      \"locked_plane\": \"xy\",\n"
        "      \"transform\": {\n"
        "        \"position\": {\"x\": 4, \"y\": 5, \"z\": 0},\n"
        "        \"rotation\": {\"x\": 0, \"y\": 0, \"z\": 0},\n"
        "        \"scale\": {\"x\": 1, \"y\": 1, \"z\": 1}\n"
        "      },\n"
        "      \"extensions\": {\n"
        "        \"line_drawing\": {\n"
        "          \"layout_object_id\": 7,\n"
        "          \"primitive_payload\": {\n"
        "            \"width\": 12,\n"
        "            \"height\": 8,\n"
        "            \"lock_to_construction_plane\": true,\n"
        "            \"lock_to_bounds\": false,\n"
        "            \"frame\": {\n"
        "              \"origin\": {\"x\": 4, \"y\": 5, \"z\": 0},\n"
        "              \"axisU\": {\"x\": 1, \"y\": 0, \"z\": 0},\n"
        "              \"axisV\": {\"x\": 0, \"y\": 1, \"z\": 0},\n"
        "              \"normal\": {\"x\": 0, \"y\": 0, \"z\": 1}\n"
        "            }\n"
        "          }\n"
        "        }\n"
        "      },\n"
        "      \"primitive\": {\n"
        "        \"kind\": \"plane_primitive\",\n"
        "        \"width\": 12,\n"
        "        \"height\": 8,\n"
        "        \"lock_to_construction_plane\": true,\n"
        "        \"lock_to_bounds\": false,\n"
        "        \"frame\": {\n"
        "          \"origin\": {\"x\": 4, \"y\": 5, \"z\": 0},\n"
        "          \"axis_u\": {\"x\": 1, \"y\": 0, \"z\": 0},\n"
        "          \"axis_v\": {\"x\": 0, \"y\": 1, \"z\": 0},\n"
        "          \"normal\": {\"x\": 0, \"y\": 0, \"z\": 1}\n"
        "        }\n"
        "      }\n"
        "    }\n"
        "  ],\n"
        "  \"extensions\": {\n"
        "    \"line_drawing\": {\n"
        "      \"scene3d\": {\n"
        "        \"authoring_bounds\": {\n"
        "          \"enabled\": true,\n"
        "          \"clamp_on_edit\": false,\n"
        "          \"min\": {\"x\": -10, \"y\": -10, \"z\": -10},\n"
        "          \"max\": {\"x\": 10, \"y\": 10, \"z\": 10}\n"
        "        },\n"
        "        \"construction_plane\": {\n"
        "          \"mode\": \"axis_aligned\",\n"
        "          \"axis\": \"xy\",\n"
        "          \"offset\": 0,\n"
        "          \"custom_frame\": {\n"
        "            \"origin\": {\"x\": 0, \"y\": 0, \"z\": 0},\n"
        "            \"axisU\": {\"x\": 1, \"y\": 0, \"z\": 0},\n"
        "            \"axisV\": {\"x\": 0, \"y\": 1, \"z\": 0},\n"
        "            \"normal\": {\"x\": 0, \"y\": 0, \"z\": 1}\n"
        "          }\n"
        "        }\n"
        "      }\n"
        "    }\n"
        "  }\n"
        "}\n";
    const char* runtime_json =
        "{\n"
        "  \"schema_variant\": \"scene_runtime_v1\",\n"
        "  \"scene_id\": \"scene_line_drawing_legacy_scene\"\n"
        "}\n";
    if (!scene_dir || !scene_dir[0]) return false;
    if (!ld_test_make_dir(scene_dir)) return false;
    if (snprintf(authoring_path, sizeof(authoring_path), "%s/scene_authoring.json", scene_dir) >= (int)sizeof(authoring_path)) {
        return false;
    }
    if (snprintf(runtime_path, sizeof(runtime_path), "%s/scene_runtime.json", scene_dir) >= (int)sizeof(runtime_path)) {
        return false;
    }
    return ld_test_write_text_file(authoring_path, authoring_json) &&
           ld_test_write_text_file(runtime_path, runtime_json);
}

static bool test_scene_menu_discovers_direct_and_grouped_scene_dirs(void) {
    char root_template[] = "/tmp/ld_scene_menu_XXXXXX";
    char* root = NULL;
    char direct_scene[512];
    char grouped_parent[512];
    char grouped_scene[512];
    char junk_parent[512];
    char junk_scene[512];
    UIPanelState* ui = NULL;
    bool found_direct = false;
    bool found_grouped = false;

    root = mkdtemp(root_template);
    TEST_ASSERT(root != NULL);
    ld_test_init_runtime();
    TEST_ASSERT(Global_SetInputRoot(root, false));

    TEST_ASSERT(snprintf(direct_scene, sizeof(direct_scene), "%s/direct_scene", root) < (int)sizeof(direct_scene));
    TEST_ASSERT(snprintf(grouped_parent, sizeof(grouped_parent), "%s/group_alpha", root) < (int)sizeof(grouped_parent));
    TEST_ASSERT(snprintf(grouped_scene, sizeof(grouped_scene), "%s/group_alpha/nested_scene", root) < (int)sizeof(grouped_scene));
    TEST_ASSERT(snprintf(junk_parent, sizeof(junk_parent), "%s/not_a_scene_group", root) < (int)sizeof(junk_parent));
    TEST_ASSERT(snprintf(junk_scene, sizeof(junk_scene), "%s/not_a_scene_group/partial_scene", root) < (int)sizeof(junk_scene));

    TEST_ASSERT(ld_test_write_scene_contract(direct_scene));
    TEST_ASSERT(ld_test_make_dir(grouped_parent));
    TEST_ASSERT(ld_test_write_scene_contract(grouped_scene));
    TEST_ASSERT(ld_test_make_dir(junk_parent));
    TEST_ASSERT(ld_test_make_dir(junk_scene));
    {
        char partial_authoring[512];
        TEST_ASSERT(snprintf(partial_authoring, sizeof(partial_authoring), "%s/scene_authoring.json", junk_scene) < (int)sizeof(partial_authoring));
        TEST_ASSERT(ld_test_write_text_file(partial_authoring, "{ }\n"));
    }

    TEST_ASSERT(UIPanel_LoadSceneFromFolderSelection(root, false));
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    TEST_ASSERT(ui->loadMenu.open);
    TEST_ASSERT(ui->loadMenu.count == 2);

    for (int i = 0; i < ui->loadMenu.count; ++i) {
        if (strcmp(ui->loadMenu.entries[i], "direct_scene") == 0 &&
            strstr(ui->loadMenu.entryPaths[i], "/direct_scene/scene_authoring.json") != NULL) {
            found_direct = true;
        }
        if (strcmp(ui->loadMenu.entries[i], "group_alpha/nested_scene") == 0 &&
            strstr(ui->loadMenu.entryPaths[i], "/group_alpha/nested_scene/scene_authoring.json") != NULL) {
            found_grouped = true;
        }
    }

    TEST_ASSERT(found_direct);
    TEST_ASSERT(found_grouped);

    UIPanel_ToggleLoadMenu();
    ld_test_shutdown_runtime();

    {
        char partial_authoring[512];
        char direct_authoring[512];
        char direct_runtime[512];
        char grouped_authoring[512];
        char grouped_runtime[512];
        TEST_ASSERT(snprintf(partial_authoring, sizeof(partial_authoring), "%s/scene_authoring.json", junk_scene) < (int)sizeof(partial_authoring));
        TEST_ASSERT(snprintf(direct_authoring, sizeof(direct_authoring), "%s/scene_authoring.json", direct_scene) < (int)sizeof(direct_authoring));
        TEST_ASSERT(snprintf(direct_runtime, sizeof(direct_runtime), "%s/scene_runtime.json", direct_scene) < (int)sizeof(direct_runtime));
        TEST_ASSERT(snprintf(grouped_authoring, sizeof(grouped_authoring), "%s/scene_authoring.json", grouped_scene) < (int)sizeof(grouped_authoring));
        TEST_ASSERT(snprintf(grouped_runtime, sizeof(grouped_runtime), "%s/scene_runtime.json", grouped_scene) < (int)sizeof(grouped_runtime));
        ld_test_remove_file_if_exists(partial_authoring);
        ld_test_remove_file_if_exists(direct_authoring);
        ld_test_remove_file_if_exists(direct_runtime);
        ld_test_remove_file_if_exists(grouped_authoring);
        ld_test_remove_file_if_exists(grouped_runtime);
    }
    ld_test_remove_dir_if_exists(junk_scene);
    ld_test_remove_dir_if_exists(junk_parent);
    ld_test_remove_dir_if_exists(grouped_scene);
    ld_test_remove_dir_if_exists(grouped_parent);
    ld_test_remove_dir_if_exists(direct_scene);
    ld_test_remove_dir_if_exists(root);
    return true;
}

static bool test_scene_menu_accepts_input_root_that_is_a_scene_dir(void) {
    char root_template[] = "/tmp/ld_scene_menu_root_scene_XXXXXX";
    char* root = NULL;
    UIPanelState* ui = NULL;

    root = mkdtemp(root_template);
    TEST_ASSERT(root != NULL);
    TEST_ASSERT(ld_test_write_scene_contract(root));
    ld_test_init_runtime();
    TEST_ASSERT(UIPanel_LoadSceneFromFolderSelection(root, false));
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    TEST_ASSERT(ui->loadMenu.open);
    TEST_ASSERT(ui->loadMenu.count == 1);
    TEST_ASSERT(ui->loadMenu.entries[0][0] != '\0');
    TEST_ASSERT(strstr(ui->loadMenu.entryPaths[0], "/scene_authoring.json") != NULL);
    TEST_ASSERT(strcmp(Global_GetInputRoot(), root) == 0);

    UIPanel_ToggleLoadMenu();
    ld_test_shutdown_runtime();

    {
        char authoring_path[512];
        char runtime_path[512];
        TEST_ASSERT(snprintf(authoring_path, sizeof(authoring_path), "%s/scene_authoring.json", root) < (int)sizeof(authoring_path));
        TEST_ASSERT(snprintf(runtime_path, sizeof(runtime_path), "%s/scene_runtime.json", root) < (int)sizeof(runtime_path));
        ld_test_remove_file_if_exists(authoring_path);
        ld_test_remove_file_if_exists(runtime_path);
    }
    ld_test_remove_dir_if_exists(root);
    return true;
}

static bool test_json_menu_discovers_directory_json_files(void) {
    char root_template[] = "/tmp/ld_json_menu_XXXXXX";
    char* root = NULL;
    char alpha_path[512];
    char beta_path[512];
    char note_path[512];
    UIPanelState* ui = NULL;
    bool found_alpha = false;
    bool found_beta = false;

    root = mkdtemp(root_template);
    TEST_ASSERT(root != NULL);
    ld_test_init_runtime();
    TEST_ASSERT(snprintf(alpha_path, sizeof(alpha_path), "%s/alpha.json", root) < (int)sizeof(alpha_path));
    TEST_ASSERT(snprintf(beta_path, sizeof(beta_path), "%s/beta.json", root) < (int)sizeof(beta_path));
    TEST_ASSERT(snprintf(note_path, sizeof(note_path), "%s/readme.txt", root) < (int)sizeof(note_path));
    TEST_ASSERT(ld_test_write_text_file(alpha_path, "{ }\n"));
    TEST_ASSERT(ld_test_write_text_file(beta_path, "{ }\n"));
    TEST_ASSERT(ld_test_write_text_file(note_path, "ignore\n"));

    TEST_ASSERT(UIPanel_LoadJsonFromFolderSelection(root, false));
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    TEST_ASSERT(ui->loadMenu.open);
    TEST_ASSERT(ui->loadMenu.mode == UI_LOAD_MENU_MODE_JSON);
    TEST_ASSERT(ui->loadMenu.count == 2);
    TEST_ASSERT(strcmp(Global_GetInputRoot(), root) == 0);

    for (int i = 0; i < ui->loadMenu.count; ++i) {
        if (strcmp(ui->loadMenu.entries[i], "alpha.json") == 0 &&
            strcmp(ui->loadMenu.entryPaths[i], alpha_path) == 0) {
            found_alpha = true;
        }
        if (strcmp(ui->loadMenu.entries[i], "beta.json") == 0 &&
            strcmp(ui->loadMenu.entryPaths[i], beta_path) == 0) {
            found_beta = true;
        }
    }

    TEST_ASSERT(found_alpha);
    TEST_ASSERT(found_beta);

    UIPanel_ToggleLoadMenu();
    ld_test_shutdown_runtime();
    ld_test_remove_file_if_exists(alpha_path);
    ld_test_remove_file_if_exists(beta_path);
    ld_test_remove_file_if_exists(note_path);
    ld_test_remove_dir_if_exists(root);
    return true;
}

static bool test_json_menu_rejects_invalid_root_without_clobbering_input_root(void) {
    char baseline_template[] = "/tmp/ld_json_menu_prev_root_XXXXXX";
    char invalid_template[] = "/tmp/ld_json_menu_invalid_root_XXXXXX";
    char stable_json[512];
    char* baseline_root = NULL;
    char* invalid_root = NULL;

    baseline_root = mkdtemp(baseline_template);
    invalid_root = mkdtemp(invalid_template);
    TEST_ASSERT(baseline_root != NULL);
    TEST_ASSERT(invalid_root != NULL);
    TEST_ASSERT(snprintf(stable_json, sizeof(stable_json), "%s/baseline.json", baseline_root) < (int)sizeof(stable_json));
    TEST_ASSERT(ld_test_write_text_file(stable_json, "{ }\n"));
    ld_test_init_runtime();
    TEST_ASSERT(UIPanel_LoadJsonFromFolderSelection(baseline_root, false));
    TEST_ASSERT(strcmp(Global_GetInputRoot(), baseline_root) == 0);
    TEST_ASSERT(!UIPanel_LoadJsonFromFolderSelection(invalid_root, false));
    TEST_ASSERT(strcmp(Global_GetInputRoot(), baseline_root) == 0);
    TEST_ASSERT(!UIPanel_Get()->loadMenu.open);

    ld_test_shutdown_runtime();
    ld_test_remove_file_if_exists(stable_json);
    ld_test_remove_dir_if_exists(invalid_root);
    ld_test_remove_dir_if_exists(baseline_root);
    return true;
}

static bool test_json_menu_click_loads_selection_and_stays_open(void) {
    char root_template[] = "/tmp/ld_json_click_menu_XXXXXX";
    char* root = NULL;
    char alpha_path[512];
    char beta_path[512];
    UIPanelState* ui = NULL;
    int alpha_index = -1;
    int beta_index = -1;
    int click_x = 0;
    int click_y = 0;

    root = mkdtemp(root_template);
    TEST_ASSERT(root != NULL);
    TEST_ASSERT(snprintf(alpha_path, sizeof(alpha_path), "%s/alpha.json", root) < (int)sizeof(alpha_path));
    TEST_ASSERT(snprintf(beta_path, sizeof(beta_path), "%s/beta.json", root) < (int)sizeof(beta_path));
    TEST_ASSERT(ld_test_write_layout_file(alpha_path, 0.0f));
    TEST_ASSERT(ld_test_write_layout_file(beta_path, 10.0f));

    ld_test_init_runtime();
    TEST_ASSERT(UIPanel_LoadJsonFromFolderSelection(root, false));
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);

    alpha_index = ld_test_find_load_menu_index(ui, "alpha.json");
    beta_index = ld_test_find_load_menu_index(ui, "beta.json");
    TEST_ASSERT(alpha_index >= 0);
    TEST_ASSERT(beta_index >= 0);

    ld_test_load_menu_click_point(ui, beta_index, &click_x, &click_y);
    TEST_ASSERT(UIPanel_HandleLoadMenuClick(click_x, click_y));
    TEST_ASSERT(ui->loadMenu.open);
    TEST_ASSERT(ui->loadMenu.activeIndex == beta_index);
    TEST_ASSERT(strcmp(Global_GetCurrentConfigPath(), beta_path) == 0);

    ld_test_load_menu_click_point(ui, alpha_index, &click_x, &click_y);
    TEST_ASSERT(UIPanel_HandleLoadMenuClick(click_x, click_y));
    TEST_ASSERT(ui->loadMenu.open);
    TEST_ASSERT(ui->loadMenu.activeIndex == alpha_index);
    TEST_ASSERT(strcmp(Global_GetCurrentConfigPath(), alpha_path) == 0);

    UIPanel_ToggleLoadMenu();
    ld_test_shutdown_runtime();
    ld_test_remove_file_if_exists(alpha_path);
    ld_test_remove_file_if_exists(beta_path);
    ld_test_remove_dir_if_exists(root);
    return true;
}

static bool test_load_menu_rect_stays_within_left_pane_and_below_button_groups(void) {
    char root_template[] = "/tmp/ld_json_menu_geom_XXXXXX";
    char* root = NULL;
    char alpha_path[512];
    UIPanelState* ui = NULL;
    CorePaneRect pane_rect = {0};
    SDL_Rect pane = {0, 0, 0, 0};
    SDL_Rect menu = {0, 0, 0, 0};
    int max_button_bottom = 0;

    root = mkdtemp(root_template);
    TEST_ASSERT(root != NULL);
    TEST_ASSERT(snprintf(alpha_path, sizeof(alpha_path), "%s/alpha.json", root) < (int)sizeof(alpha_path));
    TEST_ASSERT(ld_test_write_text_file(alpha_path, "{ }\n"));

    ld_test_init_runtime();
    TEST_ASSERT(UIPanel_LoadJsonFromFolderSelection(root, false));
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    TEST_ASSERT(LineDrawingPaneHost_GetRectForRole(Global_GetPaneHostConst(),
                                                   LINE_DRAWING_PANE_ROLE_LEFT_CONTROLS,
                                                   &pane_rect));
    pane = ld_test_pane_rect_to_sdl(pane_rect);
    menu = UIPanel_GetLoadMenuRect(ui);

    for (int i = 0; i < ui->count; ++i) {
        const UIButton* btn = &ui->buttons[i];
        if (btn->side != UI_PANEL_LEFT) continue;
        if (btn->bounds.y + btn->bounds.h > max_button_bottom) {
            max_button_bottom = btn->bounds.y + btn->bounds.h;
        }
    }

    TEST_ASSERT(menu.x >= pane.x);
    TEST_ASSERT(menu.x + menu.w <= pane.x + pane.w);
    TEST_ASSERT(menu.y > max_button_bottom);
    TEST_ASSERT(menu.y + menu.h <= pane.y + pane.h);

    UIPanel_ToggleLoadMenu();
    ld_test_shutdown_runtime();
    ld_test_remove_file_if_exists(alpha_path);
    ld_test_remove_dir_if_exists(root);
    return true;
}

static bool test_scene_folder_selection_opens_menu_for_scene_root(void) {
    char baseline_template[] = "/tmp/ld_scene_menu_prev_root_XXXXXX";
    char scene_root_template[] = "/tmp/ld_scene_menu_picker_root_XXXXXX";
    char* baseline_root = NULL;
    char* scene_root = NULL;
    char direct_scene[512];
    char grouped_parent[512];
    char grouped_scene[512];
    UIPanelState* ui = NULL;

    baseline_root = mkdtemp(baseline_template);
    scene_root = mkdtemp(scene_root_template);
    TEST_ASSERT(baseline_root != NULL);
    TEST_ASSERT(scene_root != NULL);
    ld_test_init_runtime();
    TEST_ASSERT(Global_SetInputRoot(baseline_root, false));

    TEST_ASSERT(snprintf(direct_scene, sizeof(direct_scene), "%s/direct_scene", scene_root) < (int)sizeof(direct_scene));
    TEST_ASSERT(snprintf(grouped_parent, sizeof(grouped_parent), "%s/group_beta", scene_root) < (int)sizeof(grouped_parent));
    TEST_ASSERT(snprintf(grouped_scene, sizeof(grouped_scene), "%s/group_beta/nested_scene", scene_root) < (int)sizeof(grouped_scene));
    TEST_ASSERT(ld_test_write_scene_contract(direct_scene));
    TEST_ASSERT(ld_test_make_dir(grouped_parent));
    TEST_ASSERT(ld_test_write_scene_contract(grouped_scene));

    TEST_ASSERT(UIPanel_LoadSceneFromFolderSelection(scene_root, false));
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    TEST_ASSERT(ui->loadMenu.open);
    TEST_ASSERT(ui->loadMenu.count == 2);
    TEST_ASSERT(strcmp(Global_GetInputRoot(), scene_root) == 0);

    UIPanel_ToggleLoadMenu();
    ld_test_shutdown_runtime();

    {
        char direct_authoring[512];
        char direct_runtime[512];
        char grouped_authoring[512];
        char grouped_runtime[512];
        TEST_ASSERT(snprintf(direct_authoring, sizeof(direct_authoring), "%s/scene_authoring.json", direct_scene) < (int)sizeof(direct_authoring));
        TEST_ASSERT(snprintf(direct_runtime, sizeof(direct_runtime), "%s/scene_runtime.json", direct_scene) < (int)sizeof(direct_runtime));
        TEST_ASSERT(snprintf(grouped_authoring, sizeof(grouped_authoring), "%s/scene_authoring.json", grouped_scene) < (int)sizeof(grouped_authoring));
        TEST_ASSERT(snprintf(grouped_runtime, sizeof(grouped_runtime), "%s/scene_runtime.json", grouped_scene) < (int)sizeof(grouped_runtime));
        ld_test_remove_file_if_exists(direct_authoring);
        ld_test_remove_file_if_exists(direct_runtime);
        ld_test_remove_file_if_exists(grouped_authoring);
        ld_test_remove_file_if_exists(grouped_runtime);
    }
    ld_test_remove_dir_if_exists(grouped_scene);
    ld_test_remove_dir_if_exists(grouped_parent);
    ld_test_remove_dir_if_exists(direct_scene);
    ld_test_remove_dir_if_exists(scene_root);
    ld_test_remove_dir_if_exists(baseline_root);
    return true;
}

static bool test_scene_menu_click_loads_selection_and_stays_open(void) {
    char root_template[] = "/tmp/ld_scene_click_menu_XXXXXX";
    char* root = NULL;
    Layout layout;
    LineDrawingSceneExportPaths export_paths;
    char diagnostics[256];
    UIPanelState* ui = NULL;
    int scene_index = -1;
    int click_x = 0;
    int click_y = 0;

    root = mkdtemp(root_template);
    TEST_ASSERT(root != NULL);
    Layout_Init(&layout, 1.0f);
    TEST_ASSERT(Layout_AddAnchor3(&layout, (Vec3){0.0f, 0.0f, 0.0f}) >= 0);
    TEST_ASSERT(Layout_AddAnchor3(&layout, (Vec3){2.0f, 0.0f, 1.0f}) >= 0);
    Layout_AddWall3(&layout, (Vec3){0.0f, 0.0f, 0.0f}, (Vec3){2.0f, 0.0f, 1.0f});
    TEST_ASSERT(LineDrawingSceneExport_ExportLayoutToOutputRoot(&layout,
                                                                "tests/fixtures/click scene.json",
                                                                root,
                                                                &export_paths,
                                                                diagnostics,
                                                                sizeof(diagnostics)));
    Layout_Free(&layout);

    ld_test_init_runtime();
    TEST_ASSERT(UIPanel_LoadSceneFromFolderSelection(root, false));
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);

    scene_index = ld_test_find_load_menu_index(ui, "click scene");
    TEST_ASSERT(scene_index >= 0);

    ld_test_load_menu_click_point(ui, scene_index, &click_x, &click_y);
    TEST_ASSERT(UIPanel_HandleLoadMenuClick(click_x, click_y));
    TEST_ASSERT(ui->loadMenu.open);
    TEST_ASSERT(ui->loadMenu.activeIndex == scene_index);
    TEST_ASSERT(strcmp(Global_GetCurrentSceneAuthoringPath(), export_paths.authoring_path) == 0);

    UIPanel_ToggleLoadMenu();
    ld_test_shutdown_runtime();
    ld_test_remove_file_if_exists(export_paths.authoring_path);
    ld_test_remove_file_if_exists(export_paths.runtime_path);
    ld_test_remove_dir_if_exists(export_paths.scene_dir);
    ld_test_remove_dir_if_exists(root);
    return true;
}

static bool test_scene_menu_failed_load_keeps_list_open(void) {
    char root_template[] = "/tmp/ld_scene_failed_click_menu_XXXXXX";
    char* root = NULL;
    char direct_scene[512];
    UIPanelState* ui = NULL;
    int scene_index = -1;
    int click_x = 0;
    int click_y = 0;

    root = mkdtemp(root_template);
    TEST_ASSERT(root != NULL);
    TEST_ASSERT(snprintf(direct_scene, sizeof(direct_scene), "%s/direct_scene", root) < (int)sizeof(direct_scene));
    TEST_ASSERT(ld_test_write_scene_contract(direct_scene));

    ld_test_init_runtime();
    TEST_ASSERT(UIPanel_LoadSceneFromFolderSelection(root, false));
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    scene_index = ld_test_find_load_menu_index(ui, "direct_scene");
    TEST_ASSERT(scene_index >= 0);

    ld_test_load_menu_click_point(ui, scene_index, &click_x, &click_y);
    TEST_ASSERT(UIPanel_HandleLoadMenuClick(click_x, click_y));
    TEST_ASSERT(ui->loadMenu.open);
    TEST_ASSERT(ui->loadMenu.activeIndex == -1);
    TEST_ASSERT(Global_GetCurrentSceneAuthoringPath()[0] == '\0');

    UIPanel_ToggleLoadMenu();
    ld_test_shutdown_runtime();
    {
        char authoring_path[512];
        char runtime_path[512];
        TEST_ASSERT(snprintf(authoring_path, sizeof(authoring_path), "%s/scene_authoring.json", direct_scene) < (int)sizeof(authoring_path));
        TEST_ASSERT(snprintf(runtime_path, sizeof(runtime_path), "%s/scene_runtime.json", direct_scene) < (int)sizeof(runtime_path));
        ld_test_remove_file_if_exists(authoring_path);
        ld_test_remove_file_if_exists(runtime_path);
    }
    ld_test_remove_dir_if_exists(direct_scene);
    ld_test_remove_dir_if_exists(root);
    return true;
}

static bool test_legacy_scene_menu_click_loads_selection_and_stays_open(void) {
    char root_template[] = "/tmp/ld_scene_legacy_click_menu_XXXXXX";
    char* root = NULL;
    char legacy_scene[512];
    UIPanelState* ui = NULL;
    int scene_index = -1;
    int click_x = 0;
    int click_y = 0;

    root = mkdtemp(root_template);
    TEST_ASSERT(root != NULL);
    TEST_ASSERT(snprintf(legacy_scene, sizeof(legacy_scene), "%s/legacy_scene", root) < (int)sizeof(legacy_scene));
    TEST_ASSERT(ld_test_write_legacy_scene_contract(legacy_scene));

    ld_test_init_runtime();
    TEST_ASSERT(UIPanel_LoadSceneFromFolderSelection(root, false));
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    scene_index = ld_test_find_load_menu_index(ui, "legacy_scene");
    TEST_ASSERT(scene_index >= 0);

    ld_test_load_menu_click_point(ui, scene_index, &click_x, &click_y);
    TEST_ASSERT(UIPanel_HandleLoadMenuClick(click_x, click_y));
    TEST_ASSERT(ui->loadMenu.open);
    TEST_ASSERT(ui->loadMenu.activeIndex == scene_index);
    TEST_ASSERT(strcmp(Global_GetCurrentSceneAuthoringPath(), ui->loadMenu.entryPaths[scene_index]) == 0);
    TEST_ASSERT(Global_Get()->layout.objectStore.count >= 1u);
    TEST_ASSERT(Global_Get()->layout.objectStore.items[0].objectId == 7u);

    UIPanel_ToggleLoadMenu();
    ld_test_shutdown_runtime();
    {
        char authoring_path[512];
        char runtime_path[512];
        TEST_ASSERT(snprintf(authoring_path, sizeof(authoring_path), "%s/scene_authoring.json", legacy_scene) < (int)sizeof(authoring_path));
        TEST_ASSERT(snprintf(runtime_path, sizeof(runtime_path), "%s/scene_runtime.json", legacy_scene) < (int)sizeof(runtime_path));
        ld_test_remove_file_if_exists(authoring_path);
        ld_test_remove_file_if_exists(runtime_path);
    }
    ld_test_remove_dir_if_exists(legacy_scene);
    ld_test_remove_dir_if_exists(root);
    return true;
}

static bool test_scene_folder_selection_rejects_invalid_root_without_clobbering_input_root(void) {
    char baseline_template[] = "/tmp/ld_scene_menu_stable_root_XXXXXX";
    char invalid_template[] = "/tmp/ld_scene_menu_invalid_root_XXXXXX";
    char* baseline_root = NULL;
    char* invalid_root = NULL;

    baseline_root = mkdtemp(baseline_template);
    invalid_root = mkdtemp(invalid_template);
    TEST_ASSERT(baseline_root != NULL);
    TEST_ASSERT(invalid_root != NULL);
    ld_test_init_runtime();
    TEST_ASSERT(Global_SetInputRoot(baseline_root, false));

    TEST_ASSERT(!UIPanel_LoadSceneFromFolderSelection(invalid_root, false));
    TEST_ASSERT(strcmp(Global_GetInputRoot(), baseline_root) == 0);
    TEST_ASSERT(!UIPanel_Get()->loadMenu.open);

    ld_test_shutdown_runtime();
    ld_test_remove_dir_if_exists(invalid_root);
    ld_test_remove_dir_if_exists(baseline_root);
    return true;
}

static bool test_primitive_creation_selects_useful_resize_handle(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_3D, false));

    TEST_ASSERT(UIPanel_CreatePlanePrimitiveFromActiveContext(true));
    TEST_ASSERT(state->editor.selectedObject3DId != 0u);
    TEST_ASSERT(state->editor.selectedObject3DResizeHandle ==
                PLANE_RESIZE_HANDLE_CORNER_POS_U_POS_V);
    TEST_ASSERT(state->editor.selectedObject3DPrismHandle ==
                RECT_PRISM_RESIZE_HANDLE_NONE);

    TEST_ASSERT(UIPanel_CreateRectPrismPrimitiveFromActiveContext(true));
    TEST_ASSERT(state->editor.selectedObject3DId != 0u);
    TEST_ASSERT(state->editor.selectedObject3DResizeHandle ==
                PLANE_RESIZE_HANDLE_NONE);
    TEST_ASSERT(state->editor.selectedObject3DPrismHandle ==
                RECT_PRISM_RESIZE_HANDLE_CORNER_6);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_primitive_create_button_hover_sets_placement_preview(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    UIPanelState* ui = UIPanel_Get();
    const UIButton* buttons = NULL;
    int buttonCount = 0;
    const UIButton* planeButton = NULL;
    const UIButton* prismButton = NULL;

    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_3D, false));
    buttons = UIPanel_GetButtons(ui, &buttonCount);
    for (int i = 0; i < buttonCount; ++i) {
        if (buttons[i].id == UI_BTN_CREATE_PLANE) planeButton = &buttons[i];
        if (buttons[i].id == UI_BTN_CREATE_RECT_PRISM) prismButton = &buttons[i];
    }
    TEST_ASSERT(planeButton != NULL);
    TEST_ASSERT(prismButton != NULL);

    UIPanel_HandleMouseMotion(planeButton->bounds.x + planeButton->bounds.w / 2,
                              planeButton->bounds.y + planeButton->bounds.h / 2);
    TEST_ASSERT(state->editor.primitivePlacementPreview ==
                PRIMITIVE_PLACEMENT_PREVIEW_PLANE);
    PrimitivePlacementPreview preview = {0};
    TEST_ASSERT(Editor_PrimitivePlacementPreview_Build(state,
                                                       state->editor.primitivePlacementPreview,
                                                       &preview));
    TEST_ASSERT(ld_test_nearly_equal(preview.width, fmaxf(state->grid.gridSize * 4.0f, 1.0f)));

    UIPanel_HandleMouseMotion(prismButton->bounds.x + prismButton->bounds.w / 2,
                              prismButton->bounds.y + prismButton->bounds.h / 2);
    TEST_ASSERT(state->editor.primitivePlacementPreview ==
                PRIMITIVE_PLACEMENT_PREVIEW_RECT_PRISM);

    UIPanel_HandleMouseMotion(0, 0);
    TEST_ASSERT(state->editor.primitivePlacementPreview ==
                PRIMITIVE_PLACEMENT_PREVIEW_NONE);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_fit_scene_bounds_to_selected_object_command(void) {
    ld_test_init_runtime();
    GlobalState* state = Global_Get();
    TEST_ASSERT(Global_SetSpaceMode(SPACE_MODE_3D, false));

    state->grid.gridSize = 2.0f;
    TEST_ASSERT(UIPanel_CreateRectPrismPrimitiveFromActiveContext(true));
    const Object3D* object =
        Layout_ObjectStore_FindConst(&state->layout.objectStore, state->editor.selectedObject3DId);
    Vec3 objectMin = {0};
    Vec3 objectMax = {0};
    TEST_ASSERT(Layout_Object3D_ComputeWorldAABB(object, &objectMin, &objectMax));

    TEST_ASSERT(UIPanel_FitSceneBoundsToSelectedObject());
    TEST_ASSERT(state->layout.scene3d.bounds.enabled);
    TEST_ASSERT(ld_test_vec3_nearly_equal(state->layout.scene3d.bounds.min,
                                          (Vec3){
                                              objectMin.x - state->grid.gridSize,
                                              objectMin.y - state->grid.gridSize,
                                              objectMin.z - state->grid.gridSize
                                          }));
    TEST_ASSERT(ld_test_vec3_nearly_equal(state->layout.scene3d.bounds.max,
                                          (Vec3){
                                              objectMax.x + state->grid.gridSize,
                                              objectMax.y + state->grid.gridSize,
                                              objectMax.z + state->grid.gridSize
                                          }));
    TEST_ASSERT(state->editor.selectedSceneBoundsHandle == SCENE_BOUNDS_HANDLE_NONE);

    ld_test_shutdown_runtime();
    return true;
}

bool ui_panel_scene_menu_run_tests(void) {
    const TestCase cases[] = {
        { "scene_menu_discovers_direct_and_grouped_scene_dirs",
          test_scene_menu_discovers_direct_and_grouped_scene_dirs },
        { "scene_menu_accepts_input_root_that_is_a_scene_dir",
          test_scene_menu_accepts_input_root_that_is_a_scene_dir },
        { "json_menu_discovers_directory_json_files",
          test_json_menu_discovers_directory_json_files },
        { "json_menu_rejects_invalid_root_without_clobbering_input_root",
          test_json_menu_rejects_invalid_root_without_clobbering_input_root },
        { "json_menu_click_loads_selection_and_stays_open",
          test_json_menu_click_loads_selection_and_stays_open },
        { "load_menu_rect_stays_within_left_pane_and_below_button_groups",
          test_load_menu_rect_stays_within_left_pane_and_below_button_groups },
        { "scene_folder_selection_opens_menu_for_scene_root",
          test_scene_folder_selection_opens_menu_for_scene_root },
        { "scene_menu_click_loads_selection_and_stays_open",
          test_scene_menu_click_loads_selection_and_stays_open },
        { "scene_menu_failed_load_keeps_list_open",
          test_scene_menu_failed_load_keeps_list_open },
        { "legacy_scene_menu_click_loads_selection_and_stays_open",
          test_legacy_scene_menu_click_loads_selection_and_stays_open },
        { "scene_folder_selection_rejects_invalid_root_without_clobbering_input_root",
          test_scene_folder_selection_rejects_invalid_root_without_clobbering_input_root },
        { "primitive_creation_selects_useful_resize_handle",
          test_primitive_creation_selects_useful_resize_handle },
        { "primitive_create_button_hover_sets_placement_preview",
          test_primitive_create_button_hover_sets_placement_preview },
        { "fit_scene_bounds_to_selected_object_command",
          test_fit_scene_bounds_to_selected_object_command },
    };
    return run_test_cases("UIPanelSceneMenu", cases, sizeof(cases) / sizeof(cases[0]));
}
