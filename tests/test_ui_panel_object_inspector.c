#include "test_layout_internal.h"

#include "UI/ui_panel_object_inspector.h"

static bool test_object_inspector_reserves_space_for_selected_object(void) {
    GlobalState* state = NULL;
    UIPanelState* ui = NULL;
    PlanePrimitiveCreateParams plane = {0};
    uint32_t object_id = 0u;
    bool adjusted = false;
    int reserved_height = 0;
    const UIButton* first_object_button = NULL;

    ld_test_init_runtime();
    state = Global_Get();
    TEST_ASSERT(state != NULL);

    plane = (PlanePrimitiveCreateParams){
        .width = 10.0f,
        .height = 6.0f,
        .useExplicitFrame = true,
        .explicitFrame = {
            .origin = { 2.0f, 2.0f, 0.0f },
            .axisU = { 1.0f, 0.0f, 0.0f },
            .axisV = { 0.0f, 1.0f, 0.0f },
            .normal = { 0.0f, 0.0f, 1.0f }
        },
        .lockToConstructionPlane = true,
        .lockToBounds = false
    };

    TEST_ASSERT(Layout_CreatePlanePrimitive(&state->layout, &plane, &object_id, &adjusted));
    state->editor.selectedObject3DId = object_id;

    ui = UIPanel_Get();
    TEST_ASSERT(ui != NULL);
    ui->activeRightTab = UI_PANEL_RIGHT_TAB_OBJECT;
    UIPanel_OnWindowResized(state->screenWidth, state->screenHeight);

    reserved_height = UIPanel_ObjectInspectorReservedHeight(ui);
    TEST_ASSERT(reserved_height > 0);

    for (int i = 0; i < ui->count; ++i) {
        if (ui->buttons[i].id == UI_BTN_EDIT_PRISM_WIDTH) {
            first_object_button = &ui->buttons[i];
            break;
        }
    }
    TEST_ASSERT(first_object_button != NULL);
    TEST_ASSERT(first_object_button->group == UI_PANEL_GROUP_RIGHT_PRISM);
    TEST_ASSERT(first_object_button->bounds.y >= ui->rightBodyRect.y + reserved_height);

    ld_test_shutdown_runtime();
    return true;
}

bool ui_panel_object_inspector_run_tests(void) {
    const TestCase cases[] = {
        { "object_inspector_reserves_space_for_selected_object",
          test_object_inspector_reserves_space_for_selected_object },
    };
    return run_test_cases("UIPanelObjectInspector", cases, sizeof(cases) / sizeof(cases[0]));
}
