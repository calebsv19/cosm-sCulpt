#include "test_layout_internal.h"

#include "UI/ui_panel_file_summary.h"

static bool test_file_summary_reserves_space_for_file_controls(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    int reserved_height = 0;
    const UIButton* first_file_button = NULL;

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);

    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    ui->activeLeftTab = UI_PANEL_LEFT_TAB_FILE;
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);

    reserved_height = UIPanel_FileSummaryReservedHeight(ui);
    TEST_ASSERT(reserved_height > 0);

    for (int i = 0; i < ui->count; ++i) {
        if (ui->buttons[i].id == UI_BTN_SAVE_JSON) {
            first_file_button = &ui->buttons[i];
            break;
        }
    }
    TEST_ASSERT(first_file_button != NULL);
    TEST_ASSERT(first_file_button->group == UI_PANEL_GROUP_LEFT_FILE_IO);
    TEST_ASSERT(first_file_button->bounds.y >= ui->leftBodyRect.y + reserved_height);

    ld_test_shutdown_runtime();
    return true;
}

bool ui_panel_file_summary_run_tests(void) {
    const TestCase cases[] = {
        { "file_summary_reserves_space_for_file_controls",
          test_file_summary_reserves_space_for_file_controls },
    };
    return run_test_cases("UIPanelFileSummary", cases, sizeof(cases) / sizeof(cases[0]));
}
