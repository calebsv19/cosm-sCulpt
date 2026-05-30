#include "test_layout_internal.h"

#include "UI/ui_panel_create_layout.h"
#include "UI/ui_panel_create_summary.h"

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
    TEST_ASSERT(primitives_rect.y + primitives_rect.h <= operations_rect.y ||
                primitives_rect.y + primitives_rect.h <= construction_rect.y);
    if (operations_rect.h > 0) {
        TEST_ASSERT(operations_rect.y + operations_rect.h == ui->rightBodyRect.y + ui->rightBodyRect.h);
    } else {
        TEST_ASSERT(construction_rect.y + construction_rect.h == ui->rightBodyRect.y + ui->rightBodyRect.h);
    }

    for (int i = 0; i < ui->count; ++i) {
        const UIButton* btn = &ui->buttons[i];
        if (btn->side != UI_PANEL_RIGHT) continue;
        if (btn->group != UI_PANEL_GROUP_RIGHT_PRIMITIVES &&
            btn->group != UI_PANEL_GROUP_RIGHT_CONSTRUCTION) {
            continue;
        }
        if (btn->bounds.w <= 0 || btn->bounds.h <= 0) continue;
        TEST_ASSERT(btn->bounds.y >= ui->rightBodyRect.y);
        TEST_ASSERT(btn->bounds.y + btn->bounds.h <= ui->rightBodyRect.y + ui->rightBodyRect.h);
    }

    ld_test_shutdown_runtime();
    return true;
}

static bool test_create_buttons_use_uniform_grid_rows(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    const UIButton* plane_button = NULL;
    const UIButton* prism_button = NULL;
    const UIButton* xy_button = NULL;
    const UIButton* yz_button = NULL;
    const UIButton* xz_button = NULL;
    const UIButton* neg_button = NULL;
    const UIButton* pos_button = NULL;
    const UIButton* edit_button = NULL;

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
        else if (btn->id == UI_BTN_SET_CONSTRUCTION_PLANE_XY) xy_button = btn;
        else if (btn->id == UI_BTN_SET_CONSTRUCTION_PLANE_YZ) yz_button = btn;
        else if (btn->id == UI_BTN_SET_CONSTRUCTION_PLANE_XZ) xz_button = btn;
        else if (btn->id == UI_BTN_ADJUST_CONSTRUCTION_PLANE_OFFSET_NEG) neg_button = btn;
        else if (btn->id == UI_BTN_ADJUST_CONSTRUCTION_PLANE_OFFSET_POS) pos_button = btn;
        else if (btn->id == UI_BTN_EDIT_CONSTRUCTION_PLANE_OFFSET) edit_button = btn;
    }

    TEST_ASSERT(plane_button && prism_button);
    TEST_ASSERT(xy_button && yz_button && xz_button);
    TEST_ASSERT(neg_button && pos_button && edit_button);
    TEST_ASSERT(plane_button->bounds.w == prism_button->bounds.w);
    TEST_ASSERT(xy_button->bounds.w == yz_button->bounds.w);
    TEST_ASSERT(yz_button->bounds.w == xz_button->bounds.w);
    TEST_ASSERT(neg_button->bounds.w == pos_button->bounds.w);
    TEST_ASSERT(pos_button->bounds.w == edit_button->bounds.w);

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
    };
    return run_test_cases("UIPanelCreateSummary", cases, sizeof(cases) / sizeof(cases[0]));
}
