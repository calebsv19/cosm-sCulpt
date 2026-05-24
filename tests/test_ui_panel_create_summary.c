#include "test_layout_internal.h"

#include "UI/ui_panel_create_summary.h"

static bool test_create_summary_reserves_space_for_create_controls(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    int reserved_height = 0;
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

    for (int i = 0; i < ui->count; ++i) {
        if (ui->buttons[i].id == UI_BTN_CREATE_PLANE) {
            first_create_button = &ui->buttons[i];
            break;
        }
    }
    TEST_ASSERT(first_create_button != NULL);
    TEST_ASSERT(first_create_button->group == UI_PANEL_GROUP_RIGHT_PRIMITIVES);
    TEST_ASSERT(first_create_button->bounds.y >= ui->rightBodyRect.y + reserved_height);

    ld_test_shutdown_runtime();
    return true;
}

bool ui_panel_create_summary_run_tests(void) {
    const TestCase cases[] = {
        { "create_summary_reserves_space_for_create_controls",
          test_create_summary_reserves_space_for_create_controls },
    };
    return run_test_cases("UIPanelCreateSummary", cases, sizeof(cases) / sizeof(cases[0]));
}
