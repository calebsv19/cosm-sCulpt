#include "test_layout_internal.h"

#include "UI/ui_panel_view_summary.h"

static bool test_view_summary_reserves_space_for_view_controls(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    int reserved_height = 0;
    const UIButton* first_view_button = NULL;

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);

    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    ui->activeRightTab = UI_PANEL_RIGHT_TAB_VIEW;
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);

    reserved_height = UIPanel_ViewSummaryReservedHeight(ui);
    TEST_ASSERT(reserved_height > 0);

    for (int i = 0; i < ui->count; ++i) {
        if (ui->buttons[i].id == UI_BTN_RESET_ORIGIN) {
            first_view_button = &ui->buttons[i];
            break;
        }
    }
    TEST_ASSERT(first_view_button != NULL);
    TEST_ASSERT(first_view_button->group == UI_PANEL_GROUP_RIGHT_VIEW);
    TEST_ASSERT(first_view_button->bounds.y >= ui->rightBodyRect.y + reserved_height);

    ld_test_shutdown_runtime();
    return true;
}

bool ui_panel_view_summary_run_tests(void) {
    const TestCase cases[] = {
        { "view_summary_reserves_space_for_view_controls",
          test_view_summary_reserves_space_for_view_controls },
    };
    return run_test_cases("UIPanelViewSummary", cases, sizeof(cases) / sizeof(cases[0]));
}
