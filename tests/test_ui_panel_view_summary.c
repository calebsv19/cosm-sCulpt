#include "test_layout_internal.h"

#include "UI/font_manager.h"
#include "UI/ui_panel_summary_surface.h"
#include "UI/ui_panel_view_layout.h"
#include "UI/ui_panel_view_summary.h"

static bool test_view_summary_reserves_space_for_view_controls(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    int reserved_height = 0;
    SDL_Rect summary_rect = {0, 0, 0, 0};
    SDL_Rect workspace_rect = {0, 0, 0, 0};
    SDL_Rect view_rect = {0, 0, 0, 0};
    SDL_Rect modes_rect = {0, 0, 0, 0};
    const UIButton* first_view_button = NULL;

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);

    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    UIPanel_SetActiveRightTab(ui, UI_PANEL_RIGHT_TAB_VIEW);
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);

    reserved_height = UIPanel_ViewSummaryReservedHeight(ui);
    TEST_ASSERT(reserved_height > 0);
    TEST_ASSERT(UIPanel_GetViewPaneRects(ui,
                                         &summary_rect,
                                         &workspace_rect,
                                         &view_rect,
                                         &modes_rect));
    TEST_ASSERT(summary_rect.h == reserved_height);
    TEST_ASSERT(workspace_rect.y >= summary_rect.y + summary_rect.h);
    TEST_ASSERT(view_rect.y >= workspace_rect.y + workspace_rect.h);
    TEST_ASSERT(modes_rect.y >= view_rect.y + view_rect.h);

    for (int i = 0; i < ui->count; ++i) {
        if (ui->buttons[i].id == UI_BTN_RESET_ORIGIN) {
            first_view_button = &ui->buttons[i];
            break;
        }
    }
    TEST_ASSERT(first_view_button != NULL);
    TEST_ASSERT(first_view_button->group == UI_PANEL_GROUP_RIGHT_VIEW);
    TEST_ASSERT(first_view_button->bounds.y >= view_rect.y);
    TEST_ASSERT(first_view_button->bounds.y + first_view_button->bounds.h <=
                view_rect.y + view_rect.h);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_view_layout_stays_stable_when_selection_changes(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    SDL_Rect empty_summary_rect = {0, 0, 0, 0};
    SDL_Rect empty_workspace_rect = {0, 0, 0, 0};
    SDL_Rect empty_view_rect = {0, 0, 0, 0};
    SDL_Rect empty_modes_rect = {0, 0, 0, 0};
    SDL_Rect selected_summary_rect = {0, 0, 0, 0};
    SDL_Rect selected_workspace_rect = {0, 0, 0, 0};
    SDL_Rect selected_view_rect = {0, 0, 0, 0};
    SDL_Rect selected_modes_rect = {0, 0, 0, 0};
    PlanePrimitiveCreateParams plane = {0};
    uint32_t object_id = 0u;
    bool adjusted = false;

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);

    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    UIPanel_SetActiveRightTab(ui, UI_PANEL_RIGHT_TAB_VIEW);
    state->editor.selectedObject3DId = 0u;
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);
    TEST_ASSERT(UIPanel_GetViewPaneRects(ui,
                                         &empty_summary_rect,
                                         &empty_workspace_rect,
                                         &empty_view_rect,
                                         &empty_modes_rect));

    plane = (PlanePrimitiveCreateParams){
        .width = 8.0f,
        .height = 5.0f,
        .useExplicitFrame = true,
        .explicitFrame = {
            .origin = { 1.0f, 1.0f, 0.0f },
            .axisU = { 1.0f, 0.0f, 0.0f },
            .axisV = { 0.0f, 1.0f, 0.0f },
            .normal = { 0.0f, 0.0f, 1.0f }
        },
        .lockToConstructionPlane = true,
        .lockToBounds = false
    };
    TEST_ASSERT(Layout_CreatePlanePrimitive(&state->layout, &plane, &object_id, &adjusted));
    state->editor.selectedObject3DId = object_id;
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);
    TEST_ASSERT(UIPanel_GetViewPaneRects(ui,
                                         &selected_summary_rect,
                                         &selected_workspace_rect,
                                         &selected_view_rect,
                                         &selected_modes_rect));

    TEST_ASSERT(empty_summary_rect.y == selected_summary_rect.y);
    TEST_ASSERT(empty_summary_rect.h == selected_summary_rect.h);
    TEST_ASSERT(empty_workspace_rect.y == selected_workspace_rect.y);
    TEST_ASSERT(empty_workspace_rect.h == selected_workspace_rect.h);
    TEST_ASSERT(empty_view_rect.y == selected_view_rect.y);
    TEST_ASSERT(empty_modes_rect.y == selected_modes_rect.y);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_view_sections_fit_inside_pane_and_anchor_bottom(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    SDL_Rect summary_rect = {0, 0, 0, 0};
    SDL_Rect workspace_rect = {0, 0, 0, 0};
    SDL_Rect view_rect = {0, 0, 0, 0};
    SDL_Rect modes_rect = {0, 0, 0, 0};

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);

    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    UIPanel_SetActiveRightTab(ui, UI_PANEL_RIGHT_TAB_VIEW);
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);
    TEST_ASSERT(UIPanel_GetViewPaneRects(ui,
                                         &summary_rect,
                                         &workspace_rect,
                                         &view_rect,
                                         &modes_rect));

    TEST_ASSERT(summary_rect.x >= ui->rightBodyRect.x);
    TEST_ASSERT(summary_rect.y >= ui->rightBodyRect.y);
    TEST_ASSERT(summary_rect.y + summary_rect.h <= workspace_rect.y);
    TEST_ASSERT(workspace_rect.y + workspace_rect.h <= view_rect.y);
    TEST_ASSERT(view_rect.y + view_rect.h <= modes_rect.y);
    TEST_ASSERT(modes_rect.y + modes_rect.h == ui->rightBodyRect.y + ui->rightBodyRect.h);

    for (int i = 0; i < ui->count; ++i) {
        const UIButton* btn = &ui->buttons[i];
        if (btn->side != UI_PANEL_RIGHT) continue;
        if (btn->group != UI_PANEL_GROUP_RIGHT_VIEW &&
            btn->group != UI_PANEL_GROUP_RIGHT_MODES) {
            continue;
        }
        if (btn->bounds.w <= 0 || btn->bounds.h <= 0) continue;
        TEST_ASSERT(btn->bounds.y >= ui->rightBodyRect.y);
        TEST_ASSERT(btn->bounds.y + btn->bounds.h <= ui->rightBodyRect.y + ui->rightBodyRect.h);
    }

    ld_test_shutdown_runtime();
    return true;
}

static bool test_view_buttons_use_uniform_grid_rows(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    const UIButton* reset_button = NULL;
    const UIButton* zoom_in_button = NULL;
    const UIButton* zoom_out_button = NULL;
    const UIButton* delete_button = NULL;
    const UIButton* pin_button = NULL;
    const UIButton* link_button = NULL;
    const UIButton* mode_button = NULL;

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);

    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    UIPanel_SetActiveRightTab(ui, UI_PANEL_RIGHT_TAB_VIEW);
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);

    for (int i = 0; i < ui->count; ++i) {
        const UIButton* btn = &ui->buttons[i];
        if (btn->id == UI_BTN_RESET_ORIGIN) reset_button = btn;
        else if (btn->id == UI_BTN_ZOOM_IN) zoom_in_button = btn;
        else if (btn->id == UI_BTN_ZOOM_OUT) zoom_out_button = btn;
        else if (btn->id == UI_BTN_TOGGLE_DELETE) delete_button = btn;
        else if (btn->id == UI_BTN_PIN_ANCHOR) pin_button = btn;
        else if (btn->id == UI_BTN_LINK_HANDLES) link_button = btn;
        else if (btn->id == UI_BTN_TOGGLE_SPACE_MODE) mode_button = btn;
    }

    TEST_ASSERT(reset_button && zoom_in_button && zoom_out_button);
    TEST_ASSERT(delete_button && pin_button && link_button && mode_button);
    TEST_ASSERT(reset_button->bounds.w == zoom_in_button->bounds.w);
    TEST_ASSERT(zoom_in_button->bounds.w == zoom_out_button->bounds.w);
    TEST_ASSERT(delete_button->bounds.w == pin_button->bounds.w);
    TEST_ASSERT(pin_button->bounds.w == link_button->bounds.w);
    TEST_ASSERT(mode_button->bounds.x == ui->viewPane.modesRect.x);
    TEST_ASSERT(mode_button->bounds.w == ui->viewPane.modesRect.w);

    ld_test_shutdown_runtime();
    return true;
}

static bool test_shared_summary_wrap_counts_multiple_lines_when_narrow(void) {
    TTF_Font* font = NULL;
    int wrapped_lines = 0;

    ld_test_init_runtime();
    font = FontManager_GetUIPanelFont();
    wrapped_lines = UIPanelSummary_CountWrappedLines(font,
                                                     "This middle workspace sentence should wrap when the available pane width is narrow enough.",
                                                     120);
    TEST_ASSERT(wrapped_lines >= 2);

    ld_test_shutdown_runtime();
    return true;
}

bool ui_panel_view_summary_run_tests(void) {
    const TestCase cases[] = {
        { "view_summary_reserves_space_for_view_controls",
          test_view_summary_reserves_space_for_view_controls },
        { "view_layout_stays_stable_when_selection_changes",
          test_view_layout_stays_stable_when_selection_changes },
        { "view_sections_fit_inside_pane_and_anchor_bottom",
          test_view_sections_fit_inside_pane_and_anchor_bottom },
        { "view_buttons_use_uniform_grid_rows",
          test_view_buttons_use_uniform_grid_rows },
        { "shared_summary_wrap_counts_multiple_lines_when_narrow",
          test_shared_summary_wrap_counts_multiple_lines_when_narrow },
    };
    return run_test_cases("UIPanelViewSummary", cases, sizeof(cases) / sizeof(cases[0]));
}
