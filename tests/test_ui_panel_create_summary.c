#include "test_layout_internal.h"

#include "UI/ui_panel_create_layout.h"
#include "UI/ui_panel_create_summary.h"
#include "UI/input_ui_panel.h"

static bool ld_test_find_button_center(const UIPanelState* ui,
                                       int button_id,
                                       int* out_x,
                                       int* out_y) {
    if (!ui) return false;
    for (int i = 0; i < ui->count; ++i) {
        const UIButton* button = &ui->buttons[i];
        if (button->id != button_id) continue;
        if (button->bounds.w <= 0 || button->bounds.h <= 0) return false;
        if (out_x) *out_x = button->bounds.x + (button->bounds.w / 2);
        if (out_y) *out_y = button->bounds.y + (button->bounds.h / 2);
        return true;
    }
    return false;
}

static const UIButton* ld_test_find_button(const UIPanelState* ui, int button_id) {
    if (!ui) return NULL;
    for (int i = 0; i < ui->count; ++i) {
        if (ui->buttons[i].id == button_id) return &ui->buttons[i];
    }
    return NULL;
}

static bool ld_test_rect_contains(SDL_Rect outer, SDL_Rect inner) {
    return inner.w > 0 && inner.h > 0 &&
           inner.x >= outer.x && inner.y >= outer.y &&
           inner.x + inner.w <= outer.x + outer.w &&
           inner.y + inner.h <= outer.y + outer.h;
}

static bool test_create_summary_reserves_space_for_create_controls(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    int reserved_height = 0;
    SDL_Rect summary_rect = {0, 0, 0, 0};
    SDL_Rect workspace_rect = {0, 0, 0, 0};
    SDL_Rect primitives_rect = {0, 0, 0, 0};
    SDL_Rect operations_rect = {0, 0, 0, 0};
    SDL_Rect construction_rect = {0, 0, 0, 0};
    const UIButton* first_create_button = NULL;

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);

    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    ui->activeRightTab = UI_PANEL_RIGHT_TAB_CREATE;
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);

    reserved_height = UIPanel_CreateSummaryReservedHeight(ui);
    TEST_ASSERT(reserved_height > 0);
    TEST_ASSERT(UIPanel_GetCreatePaneRects(ui,
                                           &summary_rect,
                                           &workspace_rect,
                                           &primitives_rect,
                                           &operations_rect,
                                           &construction_rect));
    TEST_ASSERT(summary_rect.h == reserved_height);
    TEST_ASSERT(workspace_rect.y >= summary_rect.y + summary_rect.h);
    TEST_ASSERT(primitives_rect.y >= workspace_rect.y + workspace_rect.h);
    TEST_ASSERT(operations_rect.y >= primitives_rect.y + primitives_rect.h ||
                construction_rect.y >= primitives_rect.y + primitives_rect.h);

    for (int i = 0; i < ui->count; ++i) {
        if (ui->buttons[i].id == UI_BTN_CREATE_PLANE) {
            first_create_button = &ui->buttons[i];
            break;
        }
    }
    TEST_ASSERT(first_create_button != NULL);
    TEST_ASSERT(first_create_button->group == UI_PANEL_GROUP_RIGHT_PRIMITIVES);
    TEST_ASSERT(first_create_button->bounds.y >= primitives_rect.y);
    TEST_ASSERT(first_create_button->bounds.y + first_create_button->bounds.h <=
                primitives_rect.y + primitives_rect.h);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_create_layout_stays_stable_when_stage_changes(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    SDL_Rect none_summary_rect = {0, 0, 0, 0};
    SDL_Rect none_workspace_rect = {0, 0, 0, 0};
    SDL_Rect none_primitives_rect = {0, 0, 0, 0};
    SDL_Rect none_operations_rect = {0, 0, 0, 0};
    SDL_Rect none_construction_rect = {0, 0, 0, 0};
    SDL_Rect prism_summary_rect = {0, 0, 0, 0};
    SDL_Rect prism_workspace_rect = {0, 0, 0, 0};
    SDL_Rect prism_primitives_rect = {0, 0, 0, 0};
    SDL_Rect prism_operations_rect = {0, 0, 0, 0};
    SDL_Rect prism_construction_rect = {0, 0, 0, 0};

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);

    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    ui->activeRightTab = UI_PANEL_RIGHT_TAB_CREATE;

    state->editor.primitivePlacementPreview = PRIMITIVE_PLACEMENT_PREVIEW_NONE;
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);
    TEST_ASSERT(UIPanel_GetCreatePaneRects(ui,
                                           &none_summary_rect,
                                           &none_workspace_rect,
                                           &none_primitives_rect,
                                           &none_operations_rect,
                                           &none_construction_rect));

    state->editor.primitivePlacementPreview = PRIMITIVE_PLACEMENT_PREVIEW_RECT_PRISM;
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);
    TEST_ASSERT(UIPanel_GetCreatePaneRects(ui,
                                           &prism_summary_rect,
                                           &prism_workspace_rect,
                                           &prism_primitives_rect,
                                           &prism_operations_rect,
                                           &prism_construction_rect));

    TEST_ASSERT(none_summary_rect.y == prism_summary_rect.y);
    TEST_ASSERT(none_summary_rect.h == prism_summary_rect.h);
    TEST_ASSERT(none_workspace_rect.y == prism_workspace_rect.y);
    TEST_ASSERT(none_workspace_rect.h == prism_workspace_rect.h);
    TEST_ASSERT(none_primitives_rect.y == prism_primitives_rect.y);
    TEST_ASSERT(none_operations_rect.y == prism_operations_rect.y);
    TEST_ASSERT(none_construction_rect.y == prism_construction_rect.y);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_create_sections_fit_inside_pane_and_anchor_bottom(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    SDL_Rect summary_rect = {0, 0, 0, 0};
    SDL_Rect workspace_rect = {0, 0, 0, 0};
    SDL_Rect primitives_rect = {0, 0, 0, 0};
    SDL_Rect operations_rect = {0, 0, 0, 0};
    SDL_Rect construction_rect = {0, 0, 0, 0};

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);

    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    ui->activeRightTab = UI_PANEL_RIGHT_TAB_CREATE;
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);
    TEST_ASSERT(UIPanel_GetCreatePaneRects(ui,
                                           &summary_rect,
                                           &workspace_rect,
                                           &primitives_rect,
                                           &operations_rect,
                                           &construction_rect));

    TEST_ASSERT(summary_rect.x >= ui->rightBodyRect.x);
    TEST_ASSERT(summary_rect.y >= ui->rightBodyRect.y);
    TEST_ASSERT(summary_rect.y + summary_rect.h <= workspace_rect.y);
    TEST_ASSERT(workspace_rect.y + workspace_rect.h <= primitives_rect.y);
    TEST_ASSERT(primitives_rect.y + primitives_rect.h <= operations_rect.y);
    if (operations_rect.h > 0) {
        TEST_ASSERT(operations_rect.y + operations_rect.h <= construction_rect.y);
    }
    TEST_ASSERT(construction_rect.y + construction_rect.h == ui->rightBodyRect.y + ui->rightBodyRect.h);

    for (int i = 0; i < ui->count; ++i) {
        const UIButton* btn = &ui->buttons[i];
        if (btn->side != UI_PANEL_RIGHT) continue;
        if (btn->group != UI_PANEL_GROUP_RIGHT_PRIMITIVES &&
            btn->group != UI_PANEL_GROUP_RIGHT_OPERATIONS &&
            btn->group != UI_PANEL_GROUP_RIGHT_CONSTRUCTION) {
            continue;
        }
        if (btn->bounds.w <= 0 || btn->bounds.h <= 0) continue;
        TEST_ASSERT(btn->bounds.y >= ui->rightBodyRect.y);
        TEST_ASSERT(btn->bounds.y + btn->bounds.h <= ui->rightBodyRect.y + ui->rightBodyRect.h);
        if (btn->group == UI_PANEL_GROUP_RIGHT_PRIMITIVES) {
            TEST_ASSERT(ld_test_rect_contains(primitives_rect, btn->bounds));
        } else if (btn->group == UI_PANEL_GROUP_RIGHT_OPERATIONS) {
            TEST_ASSERT(ld_test_rect_contains(operations_rect, btn->bounds));
        } else {
            TEST_ASSERT(ld_test_rect_contains(construction_rect, btn->bounds));
        }
    }

    ld_test_shutdown_runtime();
    return true;
}

static bool test_create_buttons_use_uniform_grid_rows(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    const UIButton* plane_button = NULL;
    const UIButton* prism_button = NULL;
    const UIButton* light_button = NULL;
    const UIButton* path_button = NULL;
    const UIButton* material_button = NULL;
    const UIButton* light_path_button = NULL;
    const UIButton* generic_path_button = NULL;
    const UIButton* xy_button = NULL;
    const UIButton* yz_button = NULL;
    const UIButton* xz_button = NULL;
    const UIButton* neg_button = NULL;
    const UIButton* pos_button = NULL;
    const UIButton* edit_button = NULL;
    const UIButton* extrude_add_button = NULL;
    const UIButton* extrude_cut_button = NULL;

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);

    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    ui->activeRightTab = UI_PANEL_RIGHT_TAB_CREATE;
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);

    for (int i = 0; i < ui->count; ++i) {
        const UIButton* btn = &ui->buttons[i];
        if (btn->id == UI_BTN_CREATE_PLANE) plane_button = btn;
        else if (btn->id == UI_BTN_CREATE_RECT_PRISM) prism_button = btn;
        else if (btn->id == UI_BTN_CREATE_LIGHT) light_button = btn;
        else if (btn->id == UI_BTN_CREATE_CAMERA_PATH) path_button = btn;
        else if (btn->id == UI_BTN_CREATE_MATERIAL) material_button = btn;
        else if (btn->id == UI_BTN_CREATE_LIGHT_PATH) light_path_button = btn;
        else if (btn->id == UI_BTN_CREATE_GENERIC_PATH) generic_path_button = btn;
        else if (btn->id == UI_BTN_SET_CONSTRUCTION_PLANE_XY) xy_button = btn;
        else if (btn->id == UI_BTN_SET_CONSTRUCTION_PLANE_YZ) yz_button = btn;
        else if (btn->id == UI_BTN_SET_CONSTRUCTION_PLANE_XZ) xz_button = btn;
        else if (btn->id == UI_BTN_ADJUST_CONSTRUCTION_PLANE_OFFSET_NEG) neg_button = btn;
        else if (btn->id == UI_BTN_ADJUST_CONSTRUCTION_PLANE_OFFSET_POS) pos_button = btn;
        else if (btn->id == UI_BTN_EDIT_CONSTRUCTION_PLANE_OFFSET) edit_button = btn;
        else if (btn->id == UI_BTN_EXTRUDE_ADD) extrude_add_button = btn;
        else if (btn->id == UI_BTN_EXTRUDE_CUT) extrude_cut_button = btn;
    }

    TEST_ASSERT(plane_button && prism_button);
    TEST_ASSERT(light_button && path_button && material_button);
    TEST_ASSERT(light_path_button && generic_path_button);
    TEST_ASSERT(xy_button && yz_button && xz_button);
    TEST_ASSERT(neg_button && pos_button && edit_button);
    TEST_ASSERT(plane_button->bounds.w == prism_button->bounds.w);
    TEST_ASSERT(light_button->bounds.w == path_button->bounds.w);
    TEST_ASSERT(path_button->bounds.w == material_button->bounds.w);
    TEST_ASSERT(light_button->bounds.y == path_button->bounds.y);
    TEST_ASSERT(path_button->bounds.y == material_button->bounds.y);
    TEST_ASSERT(light_path_button->bounds.y == generic_path_button->bounds.y);
    TEST_ASSERT(light_path_button->bounds.y > light_button->bounds.y);
    TEST_ASSERT(xy_button->bounds.w == yz_button->bounds.w);
    TEST_ASSERT(yz_button->bounds.w == xz_button->bounds.w);
    TEST_ASSERT(neg_button->bounds.w == pos_button->bounds.w);
    TEST_ASSERT(pos_button->bounds.w == edit_button->bounds.w);
    TEST_ASSERT(extrude_add_button && extrude_add_button->bounds.w == 0);
    TEST_ASSERT(extrude_cut_button && extrude_cut_button->bounds.w == 0);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_create_layout_contains_controls_across_window_matrix(void) {
    static const int window_sizes[][2] = {
        {800, 600}, {1024, 768}, {1280, 720}, {1440, 900}, {1920, 1080}
    };
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;

    ld_test_init_runtime();
    state = Global_Get();
    ui = UIPanel_Get();
    TEST_ASSERT(state != NULL && ui != NULL);
    ui->activeRightTab = UI_PANEL_RIGHT_TAB_CREATE;

    for (size_t size = 0; size < sizeof(window_sizes) / sizeof(window_sizes[0]); ++size) {
        SDL_Rect summary = {0};
        SDL_Rect workspace = {0};
        SDL_Rect primitives = {0};
        SDL_Rect operations = {0};
        SDL_Rect construction = {0};
        Global_SetWindowSize(window_sizes[size][0], window_sizes[size][1]);
        TEST_ASSERT(UIPanel_GetCreatePaneRects(ui, &summary, &workspace,
                                               &primitives, &operations,
                                               &construction));
        TEST_ASSERT(summary.y + summary.h <= workspace.y);
        TEST_ASSERT(workspace.y + workspace.h <= primitives.y);
        TEST_ASSERT(primitives.y + primitives.h <= operations.y);
        TEST_ASSERT(operations.y + operations.h <= construction.y);
        for (int i = 0; i < ui->count; ++i) {
            const UIButton* btn = &ui->buttons[i];
            if (btn->bounds.w <= 0 || btn->bounds.h <= 0) continue;
            if (btn->group == UI_PANEL_GROUP_RIGHT_PRIMITIVES) {
                TEST_ASSERT(ld_test_rect_contains(primitives, btn->bounds));
            } else if (btn->group == UI_PANEL_GROUP_RIGHT_OPERATIONS) {
                TEST_ASSERT(ld_test_rect_contains(operations, btn->bounds));
            } else if (btn->group == UI_PANEL_GROUP_RIGHT_CONSTRUCTION) {
                TEST_ASSERT(ld_test_rect_contains(construction, btn->bounds));
            }
        }
    }

    ld_test_shutdown_runtime();
    return true;
}

static bool test_create_scene_authoring_buttons_append_and_select_records(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    int light_x = 0;
    int light_y = 0;
    int path_x = 0;
    int path_y = 0;
    int material_x = 0;
    int material_y = 0;
    const UIButton* light_button = NULL;
    const UIButton* plane_button = NULL;

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);
    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    ui->activeRightTab = UI_PANEL_RIGHT_TAB_CREATE;
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);

    light_button = ld_test_find_button(ui, UI_BTN_CREATE_LIGHT);
    plane_button = ld_test_find_button(ui, UI_BTN_CREATE_PLANE);
    TEST_ASSERT(light_button && plane_button);
    TEST_ASSERT(light_button->group == UI_PANEL_GROUP_RIGHT_OPERATIONS);
    TEST_ASSERT(plane_button->group == UI_PANEL_GROUP_RIGHT_PRIMITIVES);
    TEST_ASSERT(light_button->bounds.w > 0);

    TEST_ASSERT(ld_test_find_button_center(ui, UI_BTN_CREATE_LIGHT, &light_x, &light_y));
    TEST_ASSERT(UIPanel_HandleClick(light_x, light_y));
    TEST_ASSERT(state->layout.sceneAuthoring.light_count == 2u);
    TEST_ASSERT(strcmp(state->layout.sceneAuthoring.lights[1].light_id, "light_002") == 0);
    TEST_ASSERT(state->layout.sceneAuthoring.selected_kind ==
                LINE_DRAWING_SCENE_AUTHORING_SELECTION_LIGHT);
    TEST_ASSERT(state->layout.sceneAuthoring.selected_index == 1u);
    TEST_ASSERT(state->editor.selectedObject3DId == 0u);

    TEST_ASSERT(ld_test_find_button_center(ui, UI_BTN_CREATE_CAMERA_PATH, &path_x, &path_y));
    TEST_ASSERT(UIPanel_HandleClick(path_x, path_y));
    TEST_ASSERT(state->layout.sceneAuthoring.path_count == 3u);
    TEST_ASSERT(strcmp(state->layout.sceneAuthoring.paths[2].path_id,
                       "path_camera_003") == 0);
    TEST_ASSERT(state->layout.sceneAuthoring.selected_kind ==
                LINE_DRAWING_SCENE_AUTHORING_SELECTION_PATH);
    TEST_ASSERT(state->layout.sceneAuthoring.selected_index == 2u);
    TEST_ASSERT(ld_test_find_button(ui, UI_BTN_CREATE_LIGHT_PATH) != NULL);
    TEST_ASSERT(ld_test_find_button(ui, UI_BTN_CREATE_GENERIC_PATH) != NULL);

    TEST_ASSERT(ld_test_find_button_center(ui, UI_BTN_CREATE_MATERIAL, &material_x, &material_y));
    TEST_ASSERT(UIPanel_HandleClick(material_x, material_y));
    TEST_ASSERT(state->layout.sceneAuthoring.material_count == 2u);
    TEST_ASSERT(strcmp(state->layout.sceneAuthoring.materials[1].material_id, "mat_002") == 0);
    TEST_ASSERT(state->layout.sceneAuthoring.selected_kind ==
                LINE_DRAWING_SCENE_AUTHORING_SELECTION_MATERIAL);
    TEST_ASSERT(state->layout.sceneAuthoring.selected_index == 1u);

    ld_test_shutdown_runtime();
    return true;
}

bool ui_panel_create_summary_run_tests(void) {
    const TestCase cases[] = {
        { "create_summary_reserves_space_for_create_controls",
          test_create_summary_reserves_space_for_create_controls },
        { "create_layout_stays_stable_when_stage_changes",
          test_create_layout_stays_stable_when_stage_changes },
        { "create_sections_fit_inside_pane_and_anchor_bottom",
          test_create_sections_fit_inside_pane_and_anchor_bottom },
        { "create_buttons_use_uniform_grid_rows",
          test_create_buttons_use_uniform_grid_rows },
        { "create_layout_contains_controls_across_window_matrix",
          test_create_layout_contains_controls_across_window_matrix },
        { "create_scene_authoring_buttons_append_and_select_records",
          test_create_scene_authoring_buttons_append_and_select_records },
    };
    return run_test_cases("UIPanelCreateSummary", cases, sizeof(cases) / sizeof(cases[0]));
}
